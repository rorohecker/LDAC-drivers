#pragma once
#include "avdtp.hpp"
#include <chrono>
#include <functional>
#include <algorithm>
namespace xm5 {
struct ReconnectPolicy {
    unsigned max_attempts=5;
    unsigned initial_backoff_ms=250;
    unsigned max_backoff_ms=4000;
};
// Runs start_fn until it succeeds or attempts are exhausted. Pure control flow for Fake/Windows.
class ReconnectSupervisor {
    ReconnectPolicy policy_;
public:
    explicit ReconnectSupervisor(ReconnectPolicy policy={}):policy_(policy){
        if(!policy.max_attempts || !policy.initial_backoff_ms || policy.initial_backoff_ms>policy.max_backoff_ms)
            throw Error("Invalid reconnect policy");
    }
    template<typename StartFn,typename SleepFn,typename ShouldStopFn>
    auto run(StartFn start_fn,SleepFn sleep_fn,ShouldStopFn should_stop) -> decltype(start_fn()) {
        unsigned attempt=0;
        unsigned backoff=policy_.initial_backoff_ms;
        for(;;){
            if(should_stop())throw Cancelled("Reconnect cancelled");
            try{
                return start_fn();
            }catch(const Cancelled&){throw;
            }catch(const Error&){
                ++attempt;
                if(attempt>=policy_.max_attempts)throw;
                sleep_fn(backoff);
                backoff=backoff>policy_.max_backoff_ms/2?policy_.max_backoff_ms:backoff*2;
            }
        }
    }
};
}
