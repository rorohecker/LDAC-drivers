#include "avdtp.hpp"
#include <algorithm>
#include <chrono>
namespace xm5 {
std::optional<Message> Reassembler::push(const Bytes& p) {
    auto fail=[&](const char* s)->std::optional<Message>{reset();throw Error(s);};
    if(p.empty())return fail("Empty signaling packet");
    uint8_t label=p[0]>>4,type=p[0]&3,packet=(p[0]>>2)&3;
    if(packet==0 || packet==1){
        if(remaining_)return fail("Interleaved fragmented signaling message");
        size_t header=packet==0?2:3;
        if(p.size()<header)return fail("Truncated signaling header");
        if(p[header-1]&0xc0)return fail("Reserved signaling bits set");
        message_={label,type,uint8_t(p[header-1]&0x3f),Bytes(p.begin()+header,p.end())};
        if(message_.body.size()>4096)return fail("Signaling message too large");
        if(packet==0)return message_;
        if(p[1]<2)return fail("Invalid fragment count");
        remaining_=p[1]-1;
        return {};
    }
    if(!remaining_ || label!=message_.label || type!=message_.type)
        return fail("Unexpected signaling continuation");
    if((packet==3)!=(remaining_==1))return fail("Invalid final fragment position");
    if(message_.body.size()+p.size()-1>4096)return fail("Signaling message too large");
    message_.body.insert(message_.body.end(),p.begin()+1,p.end());
    --remaining_;
    if(!remaining_)return message_;
    return {};
}
std::vector<Endpoint> endpoints(const Bytes& b){
    if(b.size()%2)throw Error("Malformed endpoint list");
    std::vector<Endpoint> result;
    for(size_t i=0;i<b.size();i+=2){
        uint8_t seid=b[i]>>2;
        if(seid==0 || seid==63 || (b[i]&1) || (b[i+1]&7))throw Error("Invalid endpoint descriptor");
        result.push_back({seid,bool(b[i]&2),bool(b[i+1]&8),uint8_t(b[i+1]>>4)});
    }
    return result;
}
bool supports_ldac_48_stereo(const Bytes& b){
    bool codec=false,transport=false; unsigned seen=0;
    for(size_t i=0;i<b.size();){
        if(b.size()-i<2)throw Error("Truncated capability header");
        unsigned category=b[i],length=b[i+1]; i+=2;
        if(length>b.size()-i)throw Error("Truncated capability value");
        if(category==1){if(length!=0 || transport)throw Error("Invalid media transport capability");transport=true;}
        if(category==7){
            if(++seen!=1 || length<2)throw Error("Invalid codec capability");
            // Audio / vendor codec / Sony vendor ID 0x0000012d / LDAC codec 0x00aa.
            const Bytes signature={0,0xff,0x2d,1,0,0,0xaa,0};
            if(length==10 && std::equal(signature.begin(),signature.end(),b.begin()+i))
                codec=(b[i+8]&0x10) && (b[i+9]&1);
        }
        i+=length;
    }
    return transport && codec;
}
Bytes ldac_configuration(){return {1,0,7,10,0,0xff,0x2d,1,0,0,0xaa,0,0x10,1};}
Bytes command(uint8_t label,uint8_t signal,const Bytes& body){
    if(label>15 || signal==0 || signal>63 || body.size()>46)throw Error("Invalid outgoing command");
    Bytes result={uint8_t(label<<4),signal}; result.insert(result.end(),body.begin(),body.end());return result;
}
Bytes media_packet(const Bytes& b,uint8_t frames,uint16_t sequence,uint32_t ts,uint32_t ssrc,uint16_t mtu){
    if(b.empty() || !frames || frames>15 || b.size()+13>mtu)throw Error("Invalid LDAC media packet size/frame count");
    Bytes p={0x80,96,uint8_t(sequence>>8),uint8_t(sequence),uint8_t(ts>>24),uint8_t(ts>>16),
        uint8_t(ts>>8),uint8_t(ts),uint8_t(ssrc>>24),uint8_t(ssrc>>16),uint8_t(ssrc>>8),uint8_t(ssrc),frames};
    p.insert(p.end(),b.begin(),b.end());return p;
}
Bytes Session::transact(uint8_t signal,const Bytes& body){
    uint8_t tag=label_; label_=(label_+1)&15;
    transport_.send(0,command(tag,signal,body));
    Reassembler assembly;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    for(unsigned count=0;count<64;++count){
        auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0)throw Error("AVDTP response timeout");
        auto m=assembly.push(transport_.receive(0,static_cast<unsigned>(left)));
        if(!m)continue;
        // Initial source-initiated implementation: terminate on peer commands;
        // do not silently accept unsupported state transitions.
        if(m->type==0)throw Error("Peer-initiated AVDTP command not implemented");
        if(m->label!=tag || m->signal!=signal)throw Error("Unexpected AVDTP transaction response");
        if(m->type!=2)throw Error("Headphones rejected AVDTP signal "+std::to_string(signal));
        return m->body;
    }
    throw Error("Too many signaling fragments");
}
uint16_t Session::start(){
    if(state_!=State::idle)throw Error("Session already active");
    try{
        if(transport_.open(0)<48)throw Error("Invalid signaling MTU");
        state_=State::signaling;
        for(auto e:endpoints(transact(1))){
            if(e.media || !e.sink || e.in_use)continue;
            if(supports_ldac_48_stereo(transact(2,{uint8_t(e.seid<<2)}))){seid_=e.seid;break;}
        }
        if(!seid_)throw Error("No free 48 kHz stereo LDAC endpoint advertised");
        Bytes config={uint8_t(seid_<<2),4}; // local source SEID 1
        auto codec=ldac_configuration();config.insert(config.end(),codec.begin(),codec.end());
        if(!transact(3,config).empty())throw Error("Malformed SetConfiguration response");
        state_=State::configured;
        if(!transact(6,{uint8_t(seid_<<2)}).empty())throw Error("Malformed Open response");
        state_=State::open;
        uint16_t mtu=transport_.open(1);
        if(mtu<679)throw Error("LDAC requires a media MTU of at least 679 bytes");
        if(!transact(7,{uint8_t(seid_<<2)}).empty())throw Error("Malformed Start response");
        state_=State::streaming;return mtu;
    }catch(...){stop();throw;}
}
void Session::stop()noexcept{
    if(state_==State::streaming){try{transact(9,{uint8_t(seid_<<2)});}catch(...){} }
    if(state_==State::open || state_==State::streaming){try{transact(8,{uint8_t(seid_<<2)});}catch(...){} }
    else if(state_==State::configured){try{transact(10,{uint8_t(seid_<<2)});}catch(...){} }
    transport_.close(1);transport_.close(0);state_=State::idle;seid_=0;
}
}
