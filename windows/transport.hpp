#pragma once
#include "../core/avdtp.hpp"
#include <windows.h>
namespace xm5 {
class DriverTransport:public Transport {
    HANDLE handle_=INVALID_HANDLE_VALUE;
public:
    explicit DriverTransport(uint64_t expected_address);
    ~DriverTransport()override;
    uint16_t open(unsigned channel)override;
    void send(unsigned channel,const Bytes& packet)override;
    Bytes receive(unsigned channel,unsigned timeout_ms)override;
    void close(unsigned channel)noexcept override;
};
}
