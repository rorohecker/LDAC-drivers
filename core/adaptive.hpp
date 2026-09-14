#pragma once
#include "encoder.hpp"
#include <algorithm>
namespace xm5 {
// Congestion heuristic for LDAC EQMID selection. Pure policy — no I/O.
struct AdaptiveSample {
    unsigned send_ms=0;
    size_t queue_bytes=0;
};
class AdaptiveController {
    int quality_;
    unsigned soften_=0,harden_=0;
    size_t queue_soft_limit_;
    unsigned send_soft_ms_;
public:
    explicit AdaptiveController(int initial_quality,
                                size_t queue_soft_limit=28800,
                                unsigned send_soft_ms=40)
        :quality_(initial_quality),queue_soft_limit_(queue_soft_limit),send_soft_ms_(send_soft_ms){
        if(quality_!=LDACBT_EQMID_HQ && quality_!=LDACBT_EQMID_SQ && quality_!=LDACBT_EQMID_MQ)
            throw Error("AdaptiveController requires hq/sq/mq EQMID");
    }
    int quality()const{return quality_;}
    // Returns true when the requested EQMID changed.
    bool observe(const AdaptiveSample& sample){
        const bool congested=sample.send_ms>=send_soft_ms_ || sample.queue_bytes>=queue_soft_limit_;
        if(congested){
            harden_=0;++soften_;
            if(soften_>=3 && quality_<LDACBT_EQMID_MQ){++quality_;soften_=0;return true;}
        }else{
            soften_=0;++harden_;
            if(harden_>=12 && quality_>LDACBT_EQMID_HQ){--quality_;harden_=0;return true;}
        }
        return false;
    }
};
}
