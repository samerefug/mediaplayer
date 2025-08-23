#ifndef VIDEOENCODE
#define VIDEOENCODE

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <functional>
extern "C"{
    #include <libavcodec/avcodec.h> 
    #include <libavutil/opt.h> 
    #include <libavutil/imgutils.h>
}
#include "BaseEncoder.hpp"


inline AVFrame* alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) exit(1);
    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;
    if (av_frame_get_buffer(frame, 0) < 0) exit(1);
    return frame;
}


class VideoEncoder:public BaseEncoder{
public:
    ~VideoEncoder(){close();}
    bool init(const char* name = nullptr, AVCodecID id = AV_CODEC_ID_NONE ,AVDictionary* opt_arg = nullptr) override;
    void close() override;
    int encode(AVFrame* encode_frame, PacketCallback callback) override;

private:
    int64_t pts;
};

#endif