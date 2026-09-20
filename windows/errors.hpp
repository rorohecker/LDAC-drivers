#pragma once
#include "../core/avdtp.hpp"
#include <windows.h>
namespace xm5 {
[[noreturn]] inline void driver_error(DWORD code,bool receive=false){
    auto message="Driver IOCTL failed, Windows error "+std::to_string(code);
    if(receive && (code==ERROR_SEM_TIMEOUT || code==ERROR_TIMEOUT))throw Timeout(message);
    if(code==ERROR_OPERATION_ABORTED)throw Cancelled(message);
    throw StreamInterrupted(message);
}
}
