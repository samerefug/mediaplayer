#ifndef VIDEOENCODE
#define VIDEOENCODE

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
extern "C"{
    #include <libavcodec/avcodec.h> 
    #include <libavutil/opt.h> 
    #include <libavutil/imgutils.h>
}


class VideoEncoder{
public:
    VideoEncoder(char* name):codec_name(name),pts(0){};
    ~VideoEncoder(){close();}
    bool init();
    void close();
    AVPacket* encode(uint8_t* yuv_data,AVPacket*pkt);

private:
    const char* codec_name;
    const AVCodec* codec;
    AVCodecContext *c = nullptr;
    AVFrame *encode_frame = nullptr;
    int64_t pts;
};

#endif