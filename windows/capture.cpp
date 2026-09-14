#include "capture.hpp"
#include <cstdio>
#include <algorithm>
namespace xm5 {
static void check(HRESULT hr,const char* action){
    if(FAILED(hr)){char code[32];std::snprintf(code,sizeof(code),"0x%08lx",static_cast<unsigned long>(hr));throw Error(std::string(action)+": "+code);}
}
Capture::Capture(){
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    Microsoft::WRL::ComPtr<IMMDevice> device;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)),"Audio enumerator");
    check(enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device),"Default playback endpoint");
    check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client_.GetAddressOf())),"Activate capture");
    WAVEFORMATEX format={};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
    format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
    check(client_->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        0,0,&format,nullptr),"Initialize 48 kHz stereo loopback");
    event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event_)throw Error("Cannot allocate audio event");
    try{
        check(client_->SetEventHandle(event_),"Audio event");
        check(client_->GetService(IID_PPV_ARGS(&capture_)),"Capture service");
    }catch(...){CloseHandle(event_);event_=nullptr;throw;}
}
Capture::~Capture(){if(client_)client_->Stop();if(event_)CloseHandle(event_);}
void Capture::run(const std::atomic_bool& stop,const std::function<void(const Bytes&)>& consume){
    check(client_->Start(),"Start capture");
    bool first_packet=true;
    while(!stop.load()){
        DWORD wait=WaitForSingleObject(event_,100);
        if(wait==WAIT_TIMEOUT)continue;
        if(wait!=WAIT_OBJECT_0)throw Error("Audio event wait failed");
        UINT32 pending=0;check(capture_->GetNextPacketSize(&pending),"Capture packet size");
        while(pending && !stop.load()){
            BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;
            check(capture_->GetBuffer(&data,&frames,&flags,nullptr,nullptr),"Capture buffer");
            Bytes pcm;
            try{
                if(frames>48000)throw Error("Unexpected capture buffer size");
                pcm.resize(size_t(frames)*4);
                if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT) && !pcm.empty()){
                    if(!data)throw Error("Missing capture samples");
                    std::copy(data,data+pcm.size(),pcm.begin());
                }
            }catch(...){capture_->ReleaseBuffer(frames);throw;}
            check(capture_->ReleaseBuffer(frames),"Release capture buffer");
            if(!first_packet && (flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY))
                throw Error("Audio discontinuity; restart the session to reset LDAC timing");
            first_packet=false;
            consume(pcm);
            check(capture_->GetNextPacketSize(&pending),"Capture packet size");
        }
    }
    check(client_->Stop(),"Stop capture");
}
}
