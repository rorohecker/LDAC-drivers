#pragma once
#include "../core/avdtp.hpp"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <atomic>
#include <functional>
namespace xm5 {
class Capture {
    Microsoft::WRL::ComPtr<IAudioClient> client_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_;
    HANDLE event_=nullptr;
public:
    Capture(); ~Capture();
    void run(const std::atomic_bool& stop,const std::function<void(const Bytes&)>& consume);
};
}
