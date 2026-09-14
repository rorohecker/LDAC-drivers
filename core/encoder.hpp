#pragma once
#include "avdtp.hpp"
#include <functional>
extern "C" {
#include "ldacBT.h"
}
namespace xm5 {
class Encoder {
    HANDLE_LDAC_BT handle_=nullptr;
    Bytes pending_;uint16_t mtu_,sequence_;uint32_t timestamp_,ssrc_;
    int quality_=LDACBT_EQMID_SQ;
    void block(void* pcm,const std::function<void(const Bytes&,unsigned)>& emit);
public:
    Encoder(uint16_t mtu,int quality,uint16_t sequence,uint32_t timestamp,uint32_t ssrc);
    ~Encoder();
    Encoder(const Encoder&)=delete;Encoder& operator=(const Encoder&)=delete;
    void feed(const Bytes& pcm,const std::function<void(const Bytes&,unsigned)>& emit);
    void finish(const std::function<void(const Bytes&,unsigned)>& emit);
    int quality()const{return quality_;}
    // Mid-stream EQMID change for adaptive bitrate. Returns false if libldac rejects it.
    bool set_quality(int quality);
};
}
