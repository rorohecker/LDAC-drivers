#include "avdtp.hpp"
#include "encoder.hpp"
#include <iostream>
#include <functional>
#include <deque>
#include <random>
#include <cmath>
#include <cstring>
using namespace xm5;
static unsigned checks=0;
static void require(bool b){++checks;if(!b)throw Error("Test assertion failed at check "+std::to_string(checks));}
static void rejects(const std::function<void()>& f){bool threw=false;try{f();}catch(const Error&){threw=true;}require(threw);}
class Fake:public Transport{
public:
    std::deque<Bytes> response;std::vector<unsigned> signals;bool opened[2]={};unsigned reject_signal=0;uint16_t mtu=1024;
    uint16_t open(unsigned c)override{opened[c]=true;return c?mtu:672;}
    void close(unsigned c)noexcept override{opened[c]=false;}
    void send(unsigned c,const Bytes& p)override{
        require(c==0);signals.push_back(p[1]);Bytes reply={uint8_t((p[0]&0xf0)|(p[1]==reject_signal?3:2)),p[1]};
        if(p[1]==reject_signal){reply.push_back(0x31);response.push_back(reply);return;}
        if(p[1]==1){reply.push_back(4);reply.push_back(8);}
        if(p[1]==2){auto caps=ldac_configuration();reply.insert(reply.end(),caps.begin(),caps.end());}
        if(p[1]==3){auto caps=ldac_configuration();require(p[2]==4 && p[3]==4);require(Bytes(p.begin()+4,p.end())==caps);}
        response.push_back(reply);
    }
    Bytes receive(unsigned c,unsigned timeout)override{
        require(c==0 && timeout>0 && timeout<=3000);
        if(response.empty())throw Error("fake timeout");
        auto p=response.front();response.pop_front();return p;
    }
};
int main(){try{
    require(supports_ldac_48_stereo(ldac_configuration()));
    auto caps=ldac_configuration();caps[caps.size()-2]=0x20;require(!supports_ldac_48_stereo(caps));
    caps=ldac_configuration();caps.back()=4;require(!supports_ldac_48_stereo(caps));
    for(size_t n=1;n<14;++n){auto p=ldac_configuration();p.resize(n);bool supported=false;try{supported=supports_ldac_48_stereo(p);}catch(const Error&){}require(!supported); }
    rejects([]{endpoints({4});});rejects([]{endpoints({0,8});});
    Reassembler r;
    auto m=r.push({0x12,1,4,8});require(m && m->label==1 && m->body==Bytes({4,8}));
    require(!r.push({0x26,3,2,1,0}));require(!r.push({0x2a,7,10}));m=r.push({0x2e,0,0xff});
    require(m && m->signal==2 && m->body==Bytes({1,0,7,10,0,0xff}));
    rejects([&]{r.push({0x2a,0});});rejects([&]{r.push({0x26,1,2});});
    require(!r.push({0x16,2,2}));rejects([&]{r.push({0x2e,0});});
    require(!r.push({0x16,2,2}));rejects([&]{r.push({0x1a,0});});
    rejects([&]{r.push(Bytes(5000,0));});
    auto p=media_packet({1,2,3},2,0x1234,0x56789abc,0x12345678,679);
    require(p==Bytes({0x80,0x60,0x12,0x34,0x56,0x78,0x9a,0xbc,0x12,0x34,0x56,0x78,2,1,2,3}));
    rejects([]{media_packet(Bytes(667),1,0,0,0,679);});rejects([]{media_packet({1},16,0,0,0,679);});
    {Fake f;Session s(f);require(s.start()==1024);require(s.state()==State::streaming);s.stop();
        require(!f.opened[0] && !f.opened[1]);require(f.signals==std::vector<unsigned>({1,2,3,6,7,9,8}));}
    for(unsigned signal:{1u,2u,3u,6u,7u}){
        Fake f;f.reject_signal=signal;Session s(f);rejects([&]{s.start();});
        require(s.state()==State::idle && !f.opened[0] && !f.opened[1]);
    }
    {Fake f;f.mtu=672;Session s(f);rejects([&]{s.start();});require(!f.opened[0] && !f.opened[1]);}
    // Real encoder -> actual RTP packetizer, across MTUs and quality modes.
    for(uint16_t mtu:{uint16_t(679),uint16_t(1024),uint16_t(4096)})for(int quality:{LDACBT_EQMID_HQ,LDACBT_EQMID_SQ,LDACBT_EQMID_MQ}){
        Encoder encoder(mtu,quality,65534,0xffffff00,123);
        uint16_t sequence=65534;uint32_t timestamp=0xffffff00;unsigned total=0;
        auto emit=[&](const Bytes& b,unsigned samples){
            require(b.size()<=mtu && b.size()>13 && b[0]==0x80 && b[1]==96);
            require(uint16_t((b[2]<<8)|b[3])==sequence++);
            uint32_t ts=(uint32_t(b[4])<<24)|(uint32_t(b[5])<<16)|(uint32_t(b[6])<<8)|b[7];
            require(ts==timestamp && samples==unsigned(b[12])*128);timestamp+=samples;total+=samples;
        };
        Bytes pcm(48000*4);
        for(size_t i=0;i<48000;++i){int16_t x=int16_t(8000*std::sin(2*3.141592653589793*440*i/48000));
            pcm[4*i]=pcm[4*i+2]=uint8_t(x);pcm[4*i+1]=pcm[4*i+3]=uint8_t(uint16_t(x)>>8);}
        // Feed irregular sample-aligned chunks to exercise partial buffering.
        for(size_t i=0;i<pcm.size();){size_t n=std::min(size_t(772),pcm.size()-i);encoder.feed(Bytes(pcm.begin()+i,pcm.begin()+i+n),emit);i+=n;}
        encoder.finish(emit);require(total>=48000 && total<=48512);
    }
    // Deterministic malformed signaling/capability corpus; sanitizer builds check memory access.
    std::mt19937 random(719);
    for(unsigned n=0;n<10000;++n){Bytes b(random()%128);for(auto& x:b)x=uint8_t(random());
        try{supports_ldac_48_stereo(b);}catch(const Error&){}
        try{endpoints(b);}catch(const Error&){}
        try{r.push(b);}catch(const Error&){}
    }
    std::cout<<"PASS: "<<checks<<" checks; negotiation, fragmentation, rejection cleanup, RTP, real encoder MTU/quality matrix, malformed corpus.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
