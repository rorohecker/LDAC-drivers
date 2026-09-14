#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <optional>
#include <string>
namespace xm5 {
using Bytes=std::vector<uint8_t>;
struct Error:std::runtime_error { using std::runtime_error::runtime_error; };
struct Message { uint8_t label=0,type=0,signal=0; Bytes body; };
class Reassembler {
    Message message_; unsigned remaining_=0;
public:
    std::optional<Message> push(const Bytes& packet);
    void reset() { remaining_=0; message_={}; }
};
struct Endpoint { uint8_t seid; bool in_use, sink; uint8_t media; };
std::vector<Endpoint> endpoints(const Bytes& bytes);
bool supports_ldac_48_stereo(const Bytes& capabilities);
Bytes ldac_configuration();
Bytes command(uint8_t label,uint8_t signal,const Bytes& body={});
Bytes media_packet(const Bytes& encoded,uint8_t frames,uint16_t sequence,
                   uint32_t timestamp,uint32_t ssrc,uint16_t mtu);
class Transport {
public:
    virtual ~Transport()=default;
    virtual uint16_t open(unsigned channel)=0; // 0 signaling, 1 media; returns outbound MTU
    virtual void send(unsigned channel,const Bytes& packet)=0;
    virtual Bytes receive(unsigned channel,unsigned timeout_ms)=0;
    virtual void close(unsigned channel) noexcept=0;
};
enum class State { idle, signaling, configured, open, streaming };
class Session {
    Transport& transport_; uint8_t label_=0,seid_=0; State state_=State::idle;
    Bytes transact(uint8_t signal,const Bytes& body={});
public:
    explicit Session(Transport& t):transport_(t){}
    ~Session(){ stop(); }
    Session(const Session&)=delete; Session& operator=(const Session&)=delete;
    uint16_t start();
    void stop() noexcept;
    State state()const{return state_;}
};
}
