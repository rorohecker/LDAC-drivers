#include "../windows/pacer.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
int main(){
    HANDLE stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stop)return 1;
    try{
        xm5::Pacer pacer(stop);using Clock=std::chrono::steady_clock;
        std::vector<double> late;
        for(int i=0;i<200;++i){
            auto target=Clock::now()+std::chrono::nanoseconds(5333333);
            pacer.until(target);
            late.push_back(std::chrono::duration<double,std::milli>(Clock::now()-target).count());
        }
        std::sort(late.begin(),late.end());
        std::cout<<"5.33 ms timer lateness: p50="<<late[100]<<" ms, p99="<<late[198]<<" ms (host scheduling, not Bluetooth latency)\n";
        SetEvent(stop);bool cancelled=false;
        try{pacer.until(Clock::now()+std::chrono::seconds(10));}catch(const xm5::Cancelled&){cancelled=true;}
        if(!cancelled)throw xm5::Error("Timer ignored cancellation");
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";CloseHandle(stop);return 1;}
    CloseHandle(stop);return 0;
}
