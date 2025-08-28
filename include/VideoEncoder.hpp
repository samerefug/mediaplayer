#ifndef VIDEOENCODE
#define VIDEOENCODE

#include <stdio.h> 
#include <stdlib.h> 
#include <string> 
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
     VideoEncoder() = default;
    ~VideoEncoder(){close();}
    bool init(const char* name = nullptr, AVCodecID id = AV_CODEC_ID_NONE ,AVDictionary* opt_arg = nullptr) override;
    void close() override;
    void setVideoParams(int width,int height,int bit_rate,int fps);
    void setAudiooParams(int width, int height, int bit_rate = 400000, int fps = 25);
    void setPixelFormat(AVPixelFormat pix_fmt) { target_pix_fmt_ = pix_fmt; }
    void setQuality(const std::string& preset = "medium", int crf = -1);
    int encode(AVFrame* encode_frame, PacketCallback callback) override;

private:
    int width_;
    int height_;
    int bit_rate_;
    int fps_;
    AVPixelFormat target_pix_fmt_ = AV_PIX_FMT_YUV420P;
    std::string preset_ = "medium";

    int crf_;
};

#endif