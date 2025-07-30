#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "baseDecoder.hpp"
 extern "C"{
    #include <libavcodec/avcodec.h>
 }



 class videoDecoder:public BaseDecoder {
    public:
        videoDecoder();
        ~videoDecoder();

        bool initialize(AVCodecID codec_id);
        bool flush();
        bool parsePacket(uint8_t *&inbuf, size_t& size,AVPacket* pkt);
        bool sendPacketAndReceiveFrame(AVPacket* pkt);
        bool isAudioDecoder() const {return false;}
        AVFrame *getFrame() const;
        int getWidth() const;
        int getHeight() const;
        AVPixelFormat getPixFmt() const;
 };

