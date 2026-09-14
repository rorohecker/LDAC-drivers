#pragma once
#include "avdtp.hpp"
#include <cstdio>
namespace xm5 {
// Minimal PCM WAV reader: 48 kHz / 16-bit / stereo only (Phase 3 container support).
inline Bytes load_wav_pcm_s16le_48k_stereo(const char* path){
    FILE* f=std::fopen(path,"rb");
    if(!f)throw Error("Cannot open WAV file");
    auto close=[&]{std::fclose(f);};
    unsigned char header[12];
    if(std::fread(header,1,12,f)!=12){close();throw Error("Truncated WAV header");}
    if(header[0]!='R'||header[1]!='I'||header[2]!='F'||header[3]!='F'||
       header[8]!='W'||header[9]!='A'||header[10]!='V'||header[11]!='E'){
        close();throw Error("Not a RIFF/WAVE file");
    }
    bool have_fmt=false; uint16_t channels=0,bits=0; uint32_t rate=0; Bytes pcm;
    for(;;){
        unsigned char chunk[8];
        if(std::fread(chunk,1,8,f)!=8){close();throw Error("Truncated WAV chunk");}
        uint32_t size=uint32_t(chunk[4])|(uint32_t(chunk[5])<<8)|(uint32_t(chunk[6])<<16)|(uint32_t(chunk[7])<<24);
        if(chunk[0]=='f'&&chunk[1]=='m'&&chunk[2]=='t'&&chunk[3]==' '){
            if(size<16){close();throw Error("Invalid fmt chunk");}
            Bytes fmt(size);
            if(std::fread(fmt.data(),1,size,f)!=size){close();throw Error("Truncated fmt chunk");}
            uint16_t format=uint16_t(fmt[0])|(uint16_t(fmt[1])<<8);
            channels=uint16_t(fmt[2])|(uint16_t(fmt[3])<<8);
            rate=uint32_t(fmt[4])|(uint32_t(fmt[5])<<8)|(uint32_t(fmt[6])<<16)|(uint32_t(fmt[7])<<24);
            bits=uint16_t(fmt[14])|(uint16_t(fmt[15])<<8);
            if(format!=1){close();throw Error("WAV must be PCM format 1");}
            have_fmt=true;
            if(size&1)std::fgetc(f);
            continue;
        }
        if(chunk[0]=='d'&&chunk[1]=='a'&&chunk[2]=='t'&&chunk[3]=='a'){
            if(!have_fmt){close();throw Error("WAV data before fmt");}
            pcm.resize(size);
            if(size && std::fread(pcm.data(),1,size,f)!=size){close();throw Error("Truncated data chunk");}
            close();
            if(channels!=2 || bits!=16 || rate!=48000)throw Error("WAV must be 48 kHz stereo 16-bit PCM");
            if(pcm.size()%4)throw Error("WAV data is not sample-aligned");
            return pcm;
        }
        if(std::fseek(f,long(size+(size&1)),SEEK_CUR)!=0){close();throw Error("Cannot skip WAV chunk");}
    }
}
}
