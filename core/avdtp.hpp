#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <optional>
#include <string>
#include <functional>
namespace xm5 {
using Bytes=std::vector<uint8_t>;
struct Error:std::runtime_error { using std::runtime_error::runtime_error; };
struct Timeout:Error { using Error::Error; };
struct Cancelled:Error { using Error::Error; };
struct StreamInterrupted:Error { using Error::Error; };
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
Bytes accept_response(uint8_t label,uint8_t signal,const Bytes& body={});
Bytes reject_response(uint8_t label,uint8_t signal,uint8_t error_code,uint8_t detail=0);
Bytes general_reject(uint8_t label,uint8_t signal);
Bytes media_packet(const Bytes& encoded,uint8_t frames,uint16_t sequence,
                   uint32_t timestamp,uint32_t ssrc,uint16_t mtu);
class Transport {
public:
    virtual ~Transport()=default;
    virtual uint16_t open(unsigned channel)=0; // 0 signaling, 1 media; returns outbound MTU
    virtual void send(unsigned channel,const Bytes& packet)=0;
    virtual Bytes receive(unsigned channel,unsigned timeout_ms)=0;
    virtual void close(unsigned channel) noexcept=0;
    virtual void cancel() noexcept {}
};
enum class State { idle, signaling, configured, open, streaming, suspended };
class Session {
    Transport& transport_; uint8_t label_=0,seid_=0; State state_=State::idle;
    Reassembler inbound_;
    std::function<bool()> cancelled_;
    Bytes transact(uint8_t signal,const Bytes& body={});
    void handle_peer(const Message& message);
    void reply_accept(uint8_t label,uint8_t signal,const Bytes& body={});
    void reply_reject(uint8_t label,uint8_t signal,uint8_t error_code,uint8_t detail=0);
    void teardown_channels() noexcept;
    bool peer_targets_local(const Bytes& body) const;
public:
    explicit Session(Transport& t,std::function<bool()> cancelled={}):transport_(t),cancelled_(std::move(cancelled)){}
    ~Session(){ stop(); }
    Session(const Session&)=delete; Session& operator=(const Session&)=delete;
    uint16_t start();
    void stop() noexcept;
    // Drain one signaling packet if available. Returns false if the session ended.
    bool pump(unsigned timeout_ms=1);
    State state()const{return state_;}
    uint8_t remote_seid()const{return seid_;}
};
}
