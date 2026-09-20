#pragma once
#include "pcm_reader.hpp"
namespace xm5 {
// Convenience for small callers; live streaming uses PcmReader directly.
inline Bytes load_wav_pcm_s16le_48k_stereo(const char* path){
    PcmReader reader(path,true);
    if(reader.remaining()>192000*60)throw Error("Use PcmReader to stream WAV files longer than one minute");
    Bytes result;result.reserve(static_cast<size_t>(reader.remaining()));
    while(reader.remaining()){auto b=reader.next();result.insert(result.end(),b.begin(),b.end());}
    return result;
}
}
