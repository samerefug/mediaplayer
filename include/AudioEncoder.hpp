#ifndef AUDIOENCODER
#define AUDIOENCODER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
 
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/common.h>
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
}

class AudioEncoder{
public:
    AudioEncoder(){};
    ~AudioEncoder(){close();}
    bool init();
    void close();
    AVPacket* encode(uint8_t* yuv_data,AVPacket*pkt);

private:
    const AVCodec* codec;
    AVCodecContext *c = nullptr;
    AVFrame *encode_frame = nullptr;
    int64_t pts;
};

#endif