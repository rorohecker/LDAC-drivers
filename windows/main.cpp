#include "transport.hpp"
#include "capture.hpp"
#include "../core/encoder.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <cctype>
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
int main(int argc,char** argv){
    if(argc<4 || argc>6 || std::string(argv[1])!="--address"){
        std::cerr<<"Usage: xm5_stream --address AA:BB:CC:DD:EE:FF --file input.s16le [hq|sq|mq]\n"
                 <<"       xm5_stream --address AA:BB:CC:DD:EE:FF --loopback [hq|sq|mq]\n"
                 <<"Requires the experimental driver, already built and bound to these headphones.\n";return 2;
    }
    try{
        bool loopback=std::string(argv[3])=="--loopback";
        if((loopback && argc>5) || (!loopback && (std::string(argv[3])!="--file" || argc<5)))throw Error("Invalid input arguments");
        std::string quality=argc>(loopback?4:5)?argv[loopback?4:5]:"sq";
        int q=quality=="hq"?LDACBT_EQMID_HQ:quality=="sq"?LDACBT_EQMID_SQ:quality=="mq"?LDACBT_EQMID_MQ:-1;
        if(q<0)throw Error("Quality must be hq, sq or mq");
        std::ifstream input;
        if(!loopback){input.open(argv[4],std::ios::binary);if(!input)throw Error("Cannot open PCM file");}
        SetConsoleCtrlHandler(control,TRUE);
        DriverTransport transport(address(argv[2]));Session session(transport);
        uint16_t mtu=session.start();
        std::random_device random;
        Encoder encoder(mtu,q,static_cast<uint16_t>(random()),random(),random());
        using Clock=std::chrono::steady_clock;
        auto next=Clock::now();
        uint64_t packets=0;
        auto emit=[&](const Bytes& packet,unsigned samples){
            if(stopping.load())return;
            auto now=Clock::now();
            if(now-next>std::chrono::milliseconds(100))next=now;
            std::this_thread::sleep_until(next);
            auto before=Clock::now();transport.send(1,packet);
            if(Clock::now()-before>std::chrono::milliseconds(100))throw Error("Bluetooth send stalled; session stopped");
            next+=std::chrono::nanoseconds(uint64_t(samples)*1000000000ull/48000);
            ++packets;
        };
        std::cout<<"AVDTP Start accepted; media MTU "<<mtu<<". Streaming at 48 kHz stereo. Ctrl+C stops.\n";
        if(!loopback){
            while(!stopping.load()){
                Bytes block(4096);input.read(reinterpret_cast<char*>(block.data()),block.size());
                auto n=input.gcount();if(input.bad())throw Error("PCM read failed");
                block.resize(static_cast<size_t>(n));if(block.empty())break;
                encoder.feed(block,emit);
            }
        }else{
            std::cout<<"Capturing the current default playback device; it may also remain audible locally.\n";
            std::mutex mutex;std::condition_variable ready;std::deque<Bytes> queue;
            size_t queued=0;bool finished=false;std::exception_ptr failure;
            std::thread producer([&]{
                HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
                try{
                    if(FAILED(init))throw Error("COM initialization failed");
                    Capture capture;
                    capture.run(stopping,[&](const Bytes& block){
                        std::lock_guard<std::mutex> lock(mutex);
                        if(queued+block.size()>38400)throw Error("Audio queue exceeded 200 ms; session stopped");
                        queue.push_back(block);queued+=block.size();ready.notify_one();
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
                        block=std::move(queue.front());queue.pop_front();queued-=block.size();}
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
