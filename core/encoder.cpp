#include "encoder.hpp"
#include <algorithm>
namespace xm5 {
Encoder::Encoder(uint16_t mtu,int quality,uint16_t sequence,uint32_t timestamp,uint32_t ssrc):
    mtu_(mtu),sequence_(sequence),timestamp_(timestamp),ssrc_(ssrc),quality_(quality){
    if(mtu<679 || mtu>4096)throw Error("Unsupported media MTU");
    if(quality!=LDACBT_EQMID_HQ && quality!=LDACBT_EQMID_SQ && quality!=LDACBT_EQMID_MQ)
        throw Error("Unsupported LDAC quality");
    handle_=ldacBT_get_handle();if(!handle_)throw Error("LDAC allocation failed");
    if(ldacBT_init_handle_encode(handle_,mtu,quality,LDACBT_CHANNEL_MODE_STEREO,LDACBT_SMPL_FMT_S16,48000)){
        ldacBT_free_handle(handle_);handle_=nullptr;throw Error("LDAC initialization failed");
    }
}
Encoder::~Encoder(){if(handle_)ldacBT_free_handle(handle_);}
bool Encoder::set_quality(int quality){
    if(quality!=LDACBT_EQMID_HQ && quality!=LDACBT_EQMID_SQ && quality!=LDACBT_EQMID_MQ)return false;
    if(quality==quality_)return true;
    if(ldacBT_set_eqmid(handle_,quality))return false;
    quality_=quality;return true;
}
void Encoder::block(void* pcm,const std::function<void(const Bytes&,unsigned)>& emit){
    unsigned char data[4096];int used=0,size=0,frames=0;
    if(ldacBT_encode(handle_,pcm,&used,data,&size,&frames))throw Error("LDAC encoding error "+std::to_string(ldacBT_get_error_code(handle_)));
    if(pcm && used!=512)throw Error("Unexpected LDAC consumption");
    if(size<0 || size>4096 || frames<0 || frames>15)throw Error("Invalid encoder result");
    if(size){
        auto packet=media_packet(Bytes(data,data+size),static_cast<uint8_t>(frames),sequence_,timestamp_,ssrc_,mtu_);
        emit(packet,static_cast<unsigned>(frames)*128);
        ++sequence_;timestamp_+=static_cast<uint32_t>(frames)*128;
    }
}
void Encoder::feed(const Bytes& pcm,const std::function<void(const Bytes&,unsigned)>& emit){
    if(pcm.size()%4)throw Error("PCM requires complete stereo samples");
    if(pcm.size()>192000)throw Error("PCM chunk exceeds one second");
    pending_.insert(pending_.end(),pcm.begin(),pcm.end());
    size_t offset=0;
    while(pending_.size()-offset>=512){block(pending_.data()+offset,emit);offset+=512;}
    pending_.erase(pending_.begin(),pending_.begin()+offset);
}
void Encoder::finish(const std::function<void(const Bytes&,unsigned)>& emit){
    if(!pending_.empty()){pending_.resize(512,0);block(pending_.data(),emit);pending_.clear();}
    for(unsigned n=0;n<32;++n){
        bool emitted=false;
        block(nullptr,[&](const Bytes& bytes,unsigned samples){emitted=true;emit(bytes,samples);});
        if(!emitted)return;
    }
    throw Error("LDAC did not drain");
}
}
