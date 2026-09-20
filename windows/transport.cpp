#include "transport.hpp"
#include "errors.hpp"
#include <setupapi.h>
#include <initguid.h>
#include "../driver/public.h"
#include <algorithm>
namespace xm5 {
static void ioctl(HANDLE h,DWORD code,void* input,DWORD insize,void* output,DWORD outsize){
    DWORD returned=0;
    if(!DeviceIoControl(h,code,input,insize,output,outsize,&returned,nullptr))
        driver_error(GetLastError(),code==XM5_IOCTL_RECV);
    if(outsize && returned!=outsize)throw Error("Unexpected driver response size");
}
DriverTransport::DriverTransport(uint64_t address){
    DWORD open_error=ERROR_SUCCESS;
    HDEVINFO info=SetupDiGetClassDevsW(&GUID_DEVINTERFACE_XM5_LDAC,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(info==INVALID_HANDLE_VALUE)throw Error("Cannot enumerate experimental driver");
    for(DWORD i=0;;++i){
        SP_DEVICE_INTERFACE_DATA iface={};iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(info,nullptr,&GUID_DEVINTERFACE_XM5_LDAC,i,&iface))break;
        DWORD needed=0;
        SetupDiGetDeviceInterfaceDetailW(info,&iface,nullptr,0,&needed,nullptr);
        if(!needed)continue;
        std::vector<unsigned char> storage(needed);
        auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(info,&iface,detail,needed,nullptr,nullptr))continue;
        HANDLE candidate=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
        if(candidate==INVALID_HANDLE_VALUE){open_error=GetLastError();continue;}
        XM5_INFO result={};DWORD returned=0;
        if(DeviceIoControl(candidate,XM5_IOCTL_INFO,nullptr,0,&result,sizeof(result),&returned,nullptr)
           && returned==sizeof(result) && result.version==XM5_ABI && result.address==address){
            handle_=candidate;break;
        }
        CloseHandle(candidate);
    }
    SetupDiDestroyDeviceInfoList(info);
    if(handle_==INVALID_HANDLE_VALUE){
        if(open_error==ERROR_ACCESS_DENIED)throw Error("XM5 driver interface access denied. The experimental client requires Administrator access.");
        throw Error("No accessible XM5 driver bound to that Bluetooth address. Build, bind, and pass smoke-info.ps1 first. Windows open error "+std::to_string(open_error));
    }
}
DriverTransport::~DriverTransport(){if(handle_!=INVALID_HANDLE_VALUE){close(1);close(0);CloseHandle(handle_);}}
uint16_t DriverTransport::open(unsigned channel){
    XM5_CHANNEL p={XM5_ABI,channel,0,0};ioctl(handle_,XM5_IOCTL_OPEN,&p,sizeof(p),&p,sizeof(p));
    if(p.version!=XM5_ABI || p.channel!=channel || p.out_mtu<48 || p.out_mtu>XM5_MAX_PACKET)throw Error("Invalid negotiated MTU");
    return static_cast<uint16_t>(p.out_mtu);
}
void DriverTransport::send(unsigned channel,const Bytes& bytes){
    if(bytes.empty() || bytes.size()>XM5_MAX_PACKET)throw Error("Packet too large");
    XM5_PACKET p={};p.version=XM5_ABI;p.channel=channel;p.timeout_ms=1000;p.size=static_cast<ULONG>(bytes.size());
    std::copy(bytes.begin(),bytes.end(),p.data);
    ioctl(handle_,XM5_IOCTL_SEND,&p,sizeof(p),&p,sizeof(p));
    if(p.size!=bytes.size())throw Error("Short Bluetooth send");
}
Bytes DriverTransport::receive(unsigned channel,unsigned timeout){
    XM5_PACKET p={};p.version=XM5_ABI;p.channel=channel;p.timeout_ms=std::clamp(timeout,1u,3000u);
    ioctl(handle_,XM5_IOCTL_RECV,&p,sizeof(p),&p,sizeof(p));
    if(p.size>XM5_MAX_PACKET)throw Error("Invalid Bluetooth receive size");
    return Bytes(p.data,p.data+p.size);
}
void DriverTransport::close(unsigned channel)noexcept{
    XM5_CHANNEL p={XM5_ABI,channel,0,0};DWORD returned;
    if(!DeviceIoControl(handle_,XM5_IOCTL_CLOSE,&p,sizeof(p),nullptr,0,&returned,nullptr)){
        DWORD error=GetLastError();if(close_error_==ERROR_SUCCESS)close_error_=error;
    }
}
}
