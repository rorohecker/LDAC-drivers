#include "transport.hpp"
#include "capture.hpp"
#include "pacer.hpp"
#include "../core/encoder.hpp"
#include "../core/adaptive.hpp"
#include "../core/audio_queue.hpp"
#include "../core/reconnect.hpp"
#include "../core/pcm_reader.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cctype>
#include <memory>
using namespace xm5;
static std::atomic_bool stopping=false;
static HANDLE stop_event=nullptr;
static BOOL WINAPI control(DWORD code){
    if(code==CTRL_C_EVENT || code==CTRL_BREAK_EVENT){stopping=true;SetEvent(stop_event);return TRUE;}return FALSE;
}
static uint64_t address(const std::string& input){
    std::string digits;
    for(unsigned char c:input){if(c==':' || c=='-')continue;if(!std::isxdigit(c))throw Error("Invalid Bluetooth address");digits+=char(c);}
    if(digits.size()!=12)throw Error("Bluetooth address requires 12 hex digits");
    return std::stoull(digits,nullptr,16);
}
static void usage(){
    std::cout<<"Usage:\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --file input.s16le [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --wav input.wav [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"  xm5_stream --address AA:BB:CC:DD:EE:FF --loopback [hq|sq|mq] [--adaptive] [--reconnect]\n"
             <<"Requires the experimental driver bound to the XM5 Audio Sink; 48 kHz stereo s16le input.\n"
             <<"--adaptive adjusts HQ/SQ/MQ. --reconnect retries complete sessions up to 8 attempts.\n"
             <<"A reconnect restarts files from the beginning and reopens the default capture endpoint.\n"
             <<"A dedicated Windows playback endpoint is not implemented. Ctrl+C stops.\n";
}
struct Options {
    uint64_t addr=0;
    enum class Input { file, wav, loopback } input=Input::file;
    std::string path;
    int quality=LDACBT_EQMID_SQ;
    bool adaptive=false,reconnect=false;
};
static Options parse(int argc,char** argv){
    if(argc<4 || std::string(argv[1])!="--address")throw Error("Invalid arguments; use --help");
    Options o;o.addr=address(argv[2]);std::string mode=argv[3];int i=4;
    if(mode=="--loopback")o.input=Options::Input::loopback;
    else if(mode=="--file" || mode=="--wav"){
        if(argc<=4)throw Error("Missing input path");
        o.input=mode=="--wav"?Options::Input::wav:Options::Input::file;o.path=argv[4];i=5;
    }else throw Error("Invalid input mode");
    if(i<argc){std::string q=argv[i];if(q=="hq"||q=="sq"||q=="mq"){
        o.quality=q=="hq"?LDACBT_EQMID_HQ:q=="sq"?LDACBT_EQMID_SQ:LDACBT_EQMID_MQ;++i;}}
    for(;i<argc;++i){std::string a=argv[i];
        if(a=="--adaptive")o.adaptive=true;
        else if(a=="--reconnect")o.reconnect=true;
        else throw Error("Unknown flag "+a);}
    return o;
}
static int stream(const Options& opt){
    // Validate the file before touching Bluetooth. A fresh reader/session on retry
    // prevents buffered samples or encoder state from crossing a reconnect.
    std::unique_ptr<PcmReader> input;
    if(opt.input!=Options::Input::loopback)input=std::make_unique<PcmReader>(opt.path.c_str(),opt.input==Options::Input::wav);
    DriverTransport transport(opt.addr);Session session(transport,[]{return stopping.load();});
    uint16_t mtu=session.start();std::random_device random;
    Encoder encoder(mtu,opt.quality,static_cast<uint16_t>(random()),random(),random());
    AdaptiveController adaptive(opt.quality);Pacer pacer(stop_event);
    using Clock=std::chrono::steady_clock;
    auto next=Clock::now();uint64_t packets=0,late=0;unsigned max_send_ms=0;
    std::mutex mutex;std::condition_variable ready;AudioQueue queue;
    bool finished=false,paused=false;std::exception_ptr failure;
    std::atomic_bool capture_stop=false;
    auto pump=[&](unsigned timeout){
        if(stopping.load())throw Cancelled("Stopped");
        if(!session.pump(timeout))throw StreamInterrupted("Session ended by peer signaling");
        std::lock_guard<std::mutex> lock(mutex);
        if(failure)std::rethrow_exception(failure);
        paused=session.state()==State::suspended;
        if(paused)queue.clear();
    };
    auto emit=[&](const Bytes& packet,unsigned samples){
        pump(1);
        bool resumed=false;
        while(session.state()==State::suspended){pump(50);resumed=true;}
        if(resumed)next=Clock::now();
        pacer.until(next);
        if(stopping.load())throw Cancelled("Stopped");
        auto before=Clock::now();transport.send(1,packet);
        auto send_ms=unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-before).count());
        max_send_ms=std::max(max_send_ms,send_ms);
        if(send_ms>100)throw StreamInterrupted("Bluetooth send stalled; resetting session timing");
        size_t queued=0;{std::lock_guard<std::mutex> lock(mutex);queued=queue.bytes();}
        if(opt.adaptive && adaptive.observe({send_ms,queued,std::max(1u,samples/48)})){
            if(!encoder.set_quality(adaptive.quality()))throw Error("Encoder rejected adaptive quality change");
            std::cout<<"Adaptive quality -> "<<adaptive.quality()<<"\n";
        }
        auto period=std::chrono::nanoseconds(uint64_t(samples)*1000000000ull/48000);
        // Rebase after substantial lateness instead of emitting catch-up bursts.
        if(before-next>period/2){next=before;++late;}
        next+=period;++packets;
    };
    std::cout<<"AVDTP Start accepted; media MTU "<<mtu<<". Streaming at 48 kHz stereo. Ctrl+C stops.\n";
    if(input){
        while(input->remaining() && !stopping.load())encoder.feed(input->next(),emit);
    }else{
        std::cout<<"Capturing the current default playback device; it may remain audible locally.\n";
        std::thread producer([&]{
            HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            try{
                if(FAILED(init))throw Error("COM initialization failed");
                Capture capture;
                capture.run(capture_stop,[&](const Bytes& block){
                    std::lock_guard<std::mutex> lock(mutex);
                    if(!paused){queue.push(block);ready.notify_one();}
                });
            }catch(...){std::lock_guard<std::mutex> lock(mutex);failure=std::current_exception();}
            if(SUCCEEDED(init))CoUninitialize();
            {std::lock_guard<std::mutex> lock(mutex);finished=true;}ready.notify_one();
        });
        try{
            while(!stopping.load()){
                pump(1);
                Bytes block;
                {std::unique_lock<std::mutex> lock(mutex);
                    ready.wait_for(lock,std::chrono::milliseconds(10),[&]{return finished || !queue.empty();});
                    if(failure)std::rethrow_exception(failure);
                    if(queue.empty()){if(finished)break;continue;}
                    block=queue.pop();}
                encoder.feed(block,emit);
            }
        }catch(...){capture_stop=true;producer.join();throw;}
        capture_stop=true;producer.join();if(failure)std::rethrow_exception(failure);
    }
    if(!stopping.load())encoder.finish(emit);
    session.stop();
    if(transport.close_error()!=ERROR_SUCCESS)throw StreamInterrupted("Driver channel close failed, Windows error "+std::to_string(transport.close_error())+"; the device may require re-enumeration");
    std::cout<<"Session closed. Media packets: "<<packets<<", pacing rebases: "<<late<<", max send: "<<max_send_ms<<" ms\n";
    return 0;
}
int main(int argc,char** argv){
    try{
        if(argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h")){usage();return 0;}
        Options opt=parse(argc,argv);
        // Reject invalid input once, rather than retrying a permanently invalid file.
        if(opt.input!=Options::Input::loopback){PcmReader validate(opt.path.c_str(),opt.input==Options::Input::wav);}
        stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stop_event)throw Error("Cannot create stop event");
        if(!SetConsoleCtrlHandler(control,TRUE))throw Error("Cannot install console stop handler");
        ReconnectSupervisor supervisor(ReconnectPolicy{opt.reconnect?8u:1u,250,4000});
        unsigned attempt=0;
        int result=supervisor.run([&]{
            ++attempt;std::cout<<"Session attempt "<<attempt<<"\n";
            try{return stream(opt);}catch(const Cancelled&){throw;}
            catch(const Error& e){std::cerr<<"Attempt "<<attempt<<": "<<e.what()<<"\n";throw;}
        },[&](unsigned ms){
            std::cerr<<"Session failed; retrying in "<<ms<<" ms\n";
            if(WaitForSingleObject(stop_event,ms)==WAIT_OBJECT_0)throw Cancelled("Stopped");
        },[]{return stopping.load();});
        SetConsoleCtrlHandler(control,FALSE);CloseHandle(stop_event);return result;
    }catch(const Cancelled& e){if(stopping.load()){std::cout<<"Stopped.\n";return 0;}
        std::cerr<<"Unexpected I/O cancellation: "<<e.what()<<"\n";return 1;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
