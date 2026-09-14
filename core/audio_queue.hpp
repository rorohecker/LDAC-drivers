#pragma once
#include "avdtp.hpp"
#include <deque>
namespace xm5 {
// Bounded PCM byte queue (default ~200 ms of 48 kHz stereo s16le = 38400 bytes).
class AudioQueue {
    std::deque<Bytes> queue_;
    size_t bytes_=0;
    size_t limit_;
public:
    explicit AudioQueue(size_t limit_bytes=38400):limit_(limit_bytes){
        if(!limit_bytes)throw Error("AudioQueue limit must be positive");
    }
    size_t bytes()const{return bytes_;}
    size_t limit()const{return limit_;}
    bool empty()const{return queue_.empty();}
    void push(Bytes block){
        if(block.empty())return;
        if(bytes_+block.size()>limit_)throw Error("Audio queue exceeded limit");
        bytes_+=block.size();
        queue_.push_back(std::move(block));
    }
    Bytes pop(){
        if(queue_.empty())throw Error("AudioQueue empty");
        Bytes out=std::move(queue_.front());queue_.pop_front();
        bytes_-=out.size();return out;
    }
};
}
