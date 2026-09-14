#include "transport.hpp"
#include "capture.hpp"
#include "../core/encoder.hpp"
#include "../core/adaptive.hpp"
#include "../core/audio_queue.hpp"
#include "../core/reconnect.hpp"
#include "../core/wav.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cctype>
#include <cstring>
using namespace xm5;
static std::atomic_bool stopping=false;
static BOOL WINAPI control(DWORD code){
    if(code==CTRL_C_EVENT || code==CTRL_BREAK_EVENT){stopping=true;return TRUE;}return FALSE;
}
static uint64_t address(const std::string& input){
    std::string digits;
    for(unsigned char c:input){if(c==':' || c=='-')continue;if(!std::isxdigit(c))throw Error("Invalid Bluetooth address");digits+=char(c);}
    if(digits.size()!=12)throw Error("Bluetooth address requires 12 hex digits");
    return std::stoull(digits,nullptr,16);
}
static void usage(){
    std::cerr<<"Usage:\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --file input.s16le [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --wav input.wav [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --loopback [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"Requires the experimental driver, already built and bound to these headphones.\n"
             <<"--adaptive enables HQ/SQ/MQ congestion backoff. --reconnect retries AVDTP Start with backoff.\n"
             <<"A dedicated Windows render endpoint is not implemented yet (Phase 3 remaining on hardware).\n";
}
struct Options {
    uint64_t addr=0;
    enum class Input { file, wav, loopback } input=Input::file;
    std::string path;
    int quality=LDACBT_EQMID_SQ;
    bool adaptive=false;
    bool reconnect=false;
};
static Options parse(int argc,char** argv){
    if(argc<4 || std::string(argv[1])!="--address"){usage();throw Error("Invalid arguments");}
    Options o;o.addr=address(argv[2]);
    std::string mode=argv[3];
    int i=4;
    if(mode=="--loopback"){o.input=Options::Input::loopback;}
    else if(mode=="--file" || mode=="--wav"){
        if(argc<=4)throw Error("Missing input path");
        o.input=mode=="--wav"?Options::Input::wav:Options::Input::file;
        o.path=argv[4];i=5;
    }else throw Error("Invalid input mode");
    if(i<argc){
        std::string q=argv[i];
        if(q=="hq"||q=="sq"||q=="mq"){
            o.quality=q=="hq"?LDACBT_EQMID_HQ:q=="sq"?LDACBT_EQMID_SQ:LDACBT_EQMID_MQ;++i;
        }
    }
    for(;i<argc;++i){
        std::string a=argv[i];
        if(a=="--adaptive")o.adaptive=true;
        else if(a=="--reconnect")o.reconnect=true;
        else throw Error("Unknown flag "+a);
    }
    return o;
}
static Bytes load_pcm(const Options& o){
    if(o.input==Options::Input::wav)return load_wav_pcm_s16le_48k_stereo(o.path.c_str());
    if(o.input==Options::Input::file){
        std::ifstream input(o.path,std::ios::binary);if(!input)throw Error("Cannot open PCM file");
        return Bytes(std::istreambuf_iterator<char>(input),{});
    }
    return {};
}
int main(int argc,char** argv){
    try{
        Options opt=parse(argc,argv);
        SetConsoleCtrlHandler(control,TRUE);
        DriverTransport transport(opt.addr);
        ReconnectPolicy policy;policy.max_attempts=opt.reconnect?8:1;
        ReconnectSupervisor supervisor(policy);
        Session session(transport);
        uint16_t mtu=supervisor.run([&]{return session.start();},
            [&](unsigned ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));},
            [&]{return stopping.load();});
        std::random_device random;
        Encoder encoder(mtu,opt.quality,static_cast<uint16_t>(random()),random(),random());
        AdaptiveController adaptive(opt.quality);
        using Clock=std::chrono::steady_clock;
        auto next=Clock::now();
        uint64_t packets=0;
        auto emit=[&](const Bytes& packet,unsigned samples){
            if(stopping.load())return;
            if(!session.pump(1))throw Error("Session ended by peer signaling");
            if(session.state()==State::suspended){
                // Wait for peer Start or local stop; keep pumping.
                while(!stopping.load() && session.state()==State::suspended){
                    if(!session.pump(50))throw Error("Session ended while suspended");
                }
                if(stopping.load() || session.state()!=State::streaming)return;
            }
            auto now=Clock::now();
            if(now-next>std::chrono::milliseconds(100))next=now;
            std::this_thread::sleep_until(next);
            auto before=Clock::now();transport.send(1,packet);
            auto send_ms=unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-before).count());
            if(send_ms>100)throw Error("Bluetooth send stalled; session stopped");
            if(opt.adaptive && adaptive.observe({send_ms,0})){
                if(encoder.set_quality(adaptive.quality()))
                    std::cout<<"Adaptive quality -> "<<adaptive.quality()<<"\n";
            }
            next+=std::chrono::nanoseconds(uint64_t(samples)*1000000000ull/48000);
            ++packets;
        };
        std::cout<<"AVDTP Start accepted; media MTU "<<mtu<<". Streaming at 48 kHz stereo. Ctrl+C stops.\n";
        if(opt.input!=Options::Input::loopback){
            Bytes pcm=load_pcm(opt);
            for(size_t i=0;i<pcm.size() && !stopping.load();){
                size_t n=std::min<size_t>(4096,pcm.size()-i);
                encoder.feed(Bytes(pcm.begin()+i,pcm.begin()+i+n),emit);i+=n;
            }
        }else{
            std::cout<<"Capturing the current default playback device; it may also remain audible locally.\n";
            std::mutex mutex;std::condition_variable ready;AudioQueue queue;bool finished=false;std::exception_ptr failure;
            std::thread producer([&]{
                HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
                try{
                    if(FAILED(init))throw Error("COM initialization failed");
                    Capture capture;
                    capture.run(stopping,[&](const Bytes& block){
                        std::lock_guard<std::mutex> lock(mutex);
                        queue.push(block);ready.notify_one();
                    });
                }catch(...){std::lock_guard<std::mutex> lock(mutex);failure=std::current_exception();}
                if(SUCCEEDED(init))CoUninitialize();
                {std::lock_guard<std::mutex> lock(mutex);finished=true;}ready.notify_one();
            });
            try{
                while(!stopping.load()){
                    Bytes block;
                    {std::unique_lock<std::mutex> lock(mutex);
                        ready.wait_for(lock,std::chrono::milliseconds(100),[&]{return finished || !queue.empty();});
                        if(failure)std::rethrow_exception(failure);
                        if(queue.empty()){if(finished)break;continue;}
                        block=queue.pop();}
                    if(opt.adaptive && adaptive.observe({0,queue.bytes()})){
                        encoder.set_quality(adaptive.quality());
                    }
                    encoder.feed(block,emit);
                }
            }catch(...){stopping=true;producer.join();throw;}
            stopping=true;producer.join();
            if(failure)std::rethrow_exception(failure);
        }
        if(!stopping.load())encoder.finish(emit);
        session.stop();std::cout<<"Session closed. Media packets sent: "<<packets<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
