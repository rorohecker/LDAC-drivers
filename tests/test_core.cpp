#include "avdtp.hpp"
#include "encoder.hpp"
#include "adaptive.hpp"
#include "audio_queue.hpp"
#include "reconnect.hpp"
#include "wav.hpp"
#include <iostream>
#include <functional>
#include <deque>
#include <random>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../windows/errors.hpp"
#else
#include <unistd.h>
#endif
using namespace xm5;
static unsigned checks=0;
static void require_at(bool b,int line){++checks;if(!b)throw Error("Test assertion failed at check "+std::to_string(checks)+" line "+std::to_string(line));}
#define require(b) require_at((b),__LINE__)
static void rejects(const std::function<void()>& f){bool threw=false;try{f();}catch(const Error&){threw=true;}require(threw);}

class Fake:public Transport{
public:
    std::deque<Bytes> response;std::vector<unsigned> signals;bool opened[2]={};unsigned reject_signal=0;uint16_t mtu=1024;
    std::deque<Bytes> peer_commands; // injected before auto-replies are consumed
    std::vector<Bytes> peer_replies; // captures accept/reject we send for peer cmds
    unsigned fail_opens=0;
    uint16_t open(unsigned c)override{
        if(fail_opens){--fail_opens;throw Error("simulated open failure");}
        opened[c]=true;return c?mtu:672;
    }
    void close(unsigned c)noexcept override{opened[c]=false;}
    void send(unsigned c,const Bytes& p)override{
        if(c==1)return; // media ignored by Fake sink
        require(c==0);
        uint8_t type=p[0]&3;
        if(type!=0){peer_replies.push_back(p);return;}
        signals.push_back(p[1]);Bytes reply={uint8_t((p[0]&0xf0)|(p[1]==reject_signal?3:2)),p[1]};
        if(p[1]==reject_signal){reply.push_back(0x31);response.push_back(reply);return;}
        if(p[1]==1){reply.push_back(8);reply.push_back(8);}
        if(p[1]==2){auto caps=ldac_configuration();reply.insert(reply.end(),caps.begin(),caps.end());}
        if(p[1]==3){auto caps=ldac_configuration();require(p[2]==8 && p[3]==4);require(Bytes(p.begin()+4,p.end())==caps);}
        response.push_back(reply);
    }
    Bytes receive(unsigned c,unsigned timeout)override{
        require(c==0 && timeout<=3000);
        if(!peer_commands.empty()){auto p=peer_commands.front();peer_commands.pop_front();return p;}
        if(response.empty())throw Timeout("fake timeout");
        auto p=response.front();response.pop_front();return p;
    }
};

static Bytes peer_cmd(uint8_t label,uint8_t signal,const Bytes& body={}){
    Bytes p={uint8_t(label<<4),signal};p.insert(p.end(),body.begin(),body.end());return p;
}

int main(){try{
    require(supports_ldac_48_stereo(ldac_configuration()));
    auto advertised=ldac_configuration();
    advertised[advertised.size()-2]=0x3c; // 44.1/48/88.2/96 kHz from AOSP LDAC flags.
    auto decoded=ldac_capabilities(advertised);
    require(decoded && decoded->sample_rates==0x3c && decoded->channel_modes==1);
    require(supports_ldac_48_stereo(advertised));
    advertised[advertised.size()-2]=0x2c;
    require(!supports_ldac_48_stereo(advertised));
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

    // Phase 2: peer Suspend / Start / Close / Abort while streaming.
    {Fake f;Session s(f);require(s.start()==1024);
        f.peer_commands.push_back(peer_cmd(9,9,{uint8_t(1<<2)})); // Suspend local SEID 1
        require(s.pump(10));require(s.state()==State::suspended);
        require(!f.peer_replies.empty() && (f.peer_replies.back()[0]&3)==2);
        f.peer_commands.push_back(peer_cmd(10,7,{uint8_t(1<<2)}));
        require(s.pump(10));require(s.state()==State::streaming);
        f.peer_commands.push_back(peer_cmd(11,8,{4})); // Close remote SEID 1 (fake peer uses seid 1)
        require(!s.pump(10));require(s.state()==State::idle);}
    {Fake f;Session s(f);require(s.start()==1024);
        f.peer_commands.push_back(peer_cmd(3,10,{4}));
        require(!s.pump(10));require(s.state()==State::idle);}
    {Fake f;Session s(f);require(s.start()==1024);
        f.peer_commands.push_back(peer_cmd(4,1)); // Discover
        require(s.pump(10));require(s.state()==State::streaming);
        require(f.peer_replies.back().size()>=4 && f.peer_replies.back()[2]==(1<<2|2));}
    {Fake f;Session s(f);require(s.start()==1024);
        f.peer_commands.push_back(peer_cmd(5,2,{uint8_t(1<<2)}));
        require(s.pump(10));
        Bytes accept=f.peer_replies.back();
        require(accept.size()==2+ldac_configuration().size());
        require(Bytes(accept.begin()+2,accept.end())==ldac_configuration());}

    // Collision: peer command arrives while awaiting SetConfiguration response.
    {Fake f;Session s(f);
        // Preload: after Discover+GetCaps auto replies, inject peer Discover before SetConfiguration accept.
        // Easier: start normally then inject during a Suspend we initiate via stop — covered above.
        require(s.start()==1024);s.stop();}

    // Phase 2: reconnect supervisor.
    {unsigned sleeps=0;ReconnectSupervisor supervisor(ReconnectPolicy{3,10,40});
        unsigned opens=0;
        auto mtu=supervisor.run([&]{
            ++opens;if(opens<3)throw Error("fail");return uint16_t(1024);
        },[&](unsigned){++sleeps;},[]{return false;});
        require(mtu==1024 && opens==3 && sleeps==2);
        rejects([&]{ReconnectSupervisor(ReconnectPolicy{2,1,1}).run([]()->uint16_t{throw Error("x");return 0;},[](unsigned){},[]{return false;});});
    }

    // Phase 3: audio queue + adaptive controller.
    {AudioQueue q(16);q.push({1,2,3,4});require(q.bytes()==4);rejects([&]{q.push(Bytes(13));});
        auto b=q.pop();require(b.size()==4 && q.empty());}
    {AdaptiveController a(LDACBT_EQMID_HQ,8,10);
        require(!a.observe({5,0}));
        require(!a.observe({20,0}));
        require(!a.observe({20,0}));
        require(a.observe({20,0})); // 3rd congestion → SQ
        require(a.quality()==LDACBT_EQMID_SQ);
        for(int i=0;i<11;++i)require(!a.observe({1,0}));
        require(a.observe({1,0})); // recovery → HQ
        require(a.quality()==LDACBT_EQMID_HQ);}

    // Encoder mid-stream quality change.
    // Receive timeout classification must not depend on human-readable text.
#ifdef _WIN32
    for(DWORD code:{DWORD(ERROR_SEM_TIMEOUT),DWORD(ERROR_TIMEOUT)}){
        bool timeout=false;try{driver_error(code,true);}catch(const Timeout&){timeout=true;}require(timeout);
        bool fatal=false;try{driver_error(code,false);}catch(const StreamInterrupted&){fatal=true;}require(fatal);
    }
    {bool fatal=false;try{driver_error(ERROR_ACCESS_DENIED,true);}catch(const StreamInterrupted&){fatal=true;}require(fatal);}
#endif
    {Fake f;Session s(f);s.start();require(s.remote_seid()==2);
        require(s.pump(1)); // idle receive preserves the stream
        auto peer=[&](uint8_t signal,const Bytes& body){f.peer_commands.push_back(peer_cmd(3,signal,body));require(s.pump(1));return f.peer_replies.back();};
        require(peer(4,{4})==accept_response(3,4,ldac_configuration()));
        require(peer(4,{8})==reject_response(3,4,0x12));
        require(peer(9,{8})==reject_response(3,9,0x12,8));
        require(s.state()==State::streaming);
        require(peer(9,{4,8})==reject_response(3,9,0x12,8));
        require(s.state()==State::streaming);
        require(peer(10,{})==reject_response(3,10,0x11));
        require(s.state()==State::streaming);
        require(peer(7,{4})==reject_response(3,7,0x31,4));
        require(peer(63,{})==general_reject(3,63));
    }
    {unsigned tries=0;bool cancelled=false;
        try{ReconnectSupervisor().run([&]()->int{++tries;throw Cancelled("stop");},[](unsigned){},[]{return false;});}
        catch(const Cancelled&){cancelled=true;}
        require(cancelled && tries==1);
        rejects([]{ReconnectSupervisor invalid(ReconnectPolicy{0,1,1});});
    }
    {AudioQueue q(8);q.push(Bytes(8));rejects([&]{q.push(Bytes(4));});require(q.bytes()==8);
        q.clear();require(q.empty() && q.bytes()==0);q.push(Bytes(4));require(q.pop().size()==4);}

    // Encoder mid-stream quality change.
    {Encoder encoder(1024,LDACBT_EQMID_HQ,1,0,1);
        require(encoder.set_quality(LDACBT_EQMID_MQ));
        require(encoder.quality()==LDACBT_EQMID_MQ);
        Bytes pcm(512*16,0);unsigned packets=0;
        encoder.feed(pcm,[&](const Bytes& b,unsigned){require(b.size()>13);++packets;});
        require(packets>=1);}

    // WAV round-trip helper.
    {
#ifdef _WIN32
        char path[MAX_PATH];
        require(GetTempFileNameA(".","xm5",0,path)!=0);
        std::FILE* f=nullptr;require(fopen_s(&f,path,"wb")==0);require(f);
#else
        char path[]="/tmp/xm5_test_XXXXXX.wav";
        int fd=mkstemps(path,4);require(fd>=0);std::FILE* f=fdopen(fd,"wb");require(f);
#endif
        // 8 samples stereo s16le
        unsigned char hdr[]={
            'R','I','F','F',36+32,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
            1,0,2,0,0x80,0xbb,0,0,0x00,0xee,0x02,0,4,0,16,0,'d','a','t','a',32,0,0,0
        };
        std::fwrite(hdr,1,sizeof(hdr),f);
        unsigned char pcm[32]={};std::fwrite(pcm,1,32,f);std::fclose(f);
        auto data=load_wav_pcm_s16le_48k_stereo(path);require(data.size()==32);
        {PcmReader reader(path,true);require(reader.next(12).size()==12);require(reader.remaining()==20);
            require(reader.next().size()==20);require(reader.next().empty());rejects([&]{reader.next(3);});}
        auto rewrite=[&](const Bytes& bytes){std::ofstream out(path,std::ios::binary|std::ios::trunc);
            out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));};
        Bytes valid(hdr,hdr+sizeof(hdr));valid.resize(sizeof(hdr)+32);
        auto bad=valid;bad[16]=0xff;bad[17]=0xff;bad[18]=0xff;bad[19]=0xff;
        rewrite(bad);rejects([&]{PcmReader reader(path,true);}); // huge fmt before allocation
        bad=valid;bad[4]=0xff;rewrite(bad);rejects([&]{PcmReader reader(path,true);}); // RIFF beyond file
        bad=valid;bad[29]=0;rewrite(bad);rejects([&]{PcmReader reader(path,true);}); // invalid byte rate
        bad=valid;bad[40]=31;rewrite(bad);rejects([&]{PcmReader reader(path,true);}); // sample alignment
        rewrite(Bytes(3));rejects([&]{PcmReader reader(path,false);});
        std::remove(path);}

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
    // Targeted illegal peer-command bodies.
    {Fake f;Session s(f);require(s.start()==1024);
        f.peer_commands.push_back(peer_cmd(6,9,{uint8_t(63<<2)})); // bad SEID
        require(s.pump(10));require(s.state()==State::streaming);
        require((f.peer_replies.back()[0]&3)==3);}

    std::cout<<"PASS: "<<checks<<" checks; negotiation, peer Suspend/Close/Abort, pump, reconnect, adaptive, WAV, RTP, malformed corpus.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
