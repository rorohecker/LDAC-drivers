#pragma once
#include "avdtp.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <cstring>
namespace xm5 {
// Validate container bounds before seeking or allocating. Unknown chunks are
// skipped without loading them into memory; stream PCM in bounded blocks.
class PcmReader {
    std::ifstream input_;
    uint64_t remaining_=0;
    static uint32_t u32(const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
    void read(void* p,size_t n){
        if(!input_.read(static_cast<char*>(p),static_cast<std::streamsize>(n)))throw Error("Truncated audio file");
    }
public:
    PcmReader(const char* path,bool wav):input_(path,std::ios::binary){
        if(!input_)throw Error("Cannot open audio file");
        input_.seekg(0,std::ios::end);
        auto end=input_.tellg();if(end<0)throw Error("Cannot size audio file");
        uint64_t size=static_cast<uint64_t>(end);input_.seekg(0);
        if(!wav){remaining_=size;if(size%4)throw Error("PCM must contain whole stereo s16le samples");return;}
        std::array<unsigned char,16> b{};
        read(b.data(),12);
        if(std::memcmp(b.data(),"RIFF",4)||std::memcmp(b.data()+8,"WAVE",4))throw Error("Not a RIFF/WAVE file");
        uint64_t limit=uint64_t(u32(b.data()+4))+8,pos=12;
        if(limit<12 || limit>size)throw Error("Invalid RIFF length");
        bool fmt=false;
        while(pos<limit){
            if(limit-pos<8)throw Error("Truncated WAV chunk header");
            read(b.data(),8);pos+=8;
            uint64_t n=u32(b.data()+4),padded=n+(n&1);
            if(padded>limit-pos)throw Error("WAV chunk exceeds RIFF bounds");
            if(!std::memcmp(b.data(),"fmt ",4)){
                if(fmt || n<16)throw Error("Invalid or duplicate WAV fmt chunk");
                read(b.data(),16);
                if(b[0]!=1 || b[1]!=0 || b[2]!=2 || b[3]!=0 || u32(b.data()+4)!=48000 ||
                   u32(b.data()+8)!=192000 || b[12]!=4 || b[13]!=0 || b[14]!=16 || b[15]!=0)
                    throw Error("WAV must be 48 kHz stereo 16-bit PCM with valid byte rate and alignment");
                fmt=true;
            }else if(!std::memcmp(b.data(),"data",4)){
                if(!fmt || n%4)throw Error("WAV data requires fmt and whole stereo samples");
                remaining_=n;return;
            }
            pos+=padded;input_.seekg(static_cast<std::streamoff>(pos));
            if(!input_)throw Error("Cannot seek WAV chunk");
        }
        throw Error("WAV has no data chunk");
    }
    uint64_t remaining()const{return remaining_;}
    Bytes next(size_t max_bytes=4096){
        if(!max_bytes || max_bytes%4 || max_bytes>192000)throw Error("Invalid PCM read size");
        Bytes result(static_cast<size_t>(std::min<uint64_t>(remaining_,max_bytes)));
        if(!result.empty())read(result.data(),result.size());
        remaining_-=result.size();return result;
    }
};
}
