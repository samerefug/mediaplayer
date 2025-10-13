#ifndef VIDEODECODER
#define VIDEODECODER

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "BaseDecoder.hpp"
 extern "C"{
    #include <libavcodec/avcodec.h>
 }

 class VideoDecoder:public BaseDecoder {
    public:
        VideoDecoder();
        ~VideoDecoder();
        int getWidth() const;
        int getHeight() const;
        AVPixelFormat getPixFmt() const;
        double getFrameRate() const;
 };

#endif