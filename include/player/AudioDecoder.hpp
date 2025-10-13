#ifndef AUDIODECODER
#define AUDIODECODER

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "BaseDecoder.hpp"
extern "C"{
    #include <libavutil/frame.h>
    #include <libavutil/mem.h>
    #include <libavcodec/avcodec.h>
}

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096


class AudioDecoder :public BaseDecoder{
    public:
        AudioDecoder();
        ~AudioDecoder();

        bool initialize(AVCodecID codec_id);
        AVSampleFormat getSampleFormat() const;
        int getBytes_per_sample()const;
        int getChannels()const;
        int getSampleRate() const;
        AVRational getTimeBase() const;
        
};

#endif