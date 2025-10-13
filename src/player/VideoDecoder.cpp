#include "VideoDecoder.hpp"
#include <iostream>
VideoDecoder::VideoDecoder(){}

VideoDecoder::~VideoDecoder(){}


int VideoDecoder::getWidth() const {
    return c->width;
}

int VideoDecoder::getHeight() const {
    return c->height;
}

AVPixelFormat VideoDecoder::getPixFmt() const {
     return c ? c->pix_fmt : AV_PIX_FMT_NONE;
}
