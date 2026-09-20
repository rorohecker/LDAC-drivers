#pragma once
#include "../core/avdtp.hpp"
#include <windows.h>
#include <avrt.h>
#include <chrono>
namespace xm5 {
class Pacer {
    HANDLE timer_=nullptr,mmcss_=nullptr,stop_;
public:
    explicit Pacer(HANDLE stop):stop_(stop){
        timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(!timer_)timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        if(!timer_)throw Error("Cannot create media pacing timer");
        DWORD task=0;mmcss_=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
    }
    ~Pacer(){if(mmcss_)AvRevertMmThreadCharacteristics(mmcss_);if(timer_)CloseHandle(timer_);}
    Pacer(const Pacer&)=delete;Pacer& operator=(const Pacer&)=delete;
    void until(std::chrono::steady_clock::time_point deadline){
        auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(ns<=0)return;
        LARGE_INTEGER due;due.QuadPart=-((ns+99)/100);
        if(!SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))throw Error("Cannot arm media pacing timer");
        HANDLE waits[]={stop_,timer_};
        auto result=WaitForMultipleObjects(2,waits,FALSE,INFINITE);
        if(result==WAIT_OBJECT_0)throw Cancelled("Stopped");
        if(result!=WAIT_OBJECT_0+1)throw Error("Media pacing wait failed");
    }
};
}
