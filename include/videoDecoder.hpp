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
        bool loadPacket(uint8_t *&inbuf, size_t& size);
        bool sendPacketAndReceiveFrame();
        bool isAudioDecoder() const {return false;}
        AVFrame *getFrame() const;
        int getWidth() const;
        int getHeight() const;
        AVPixelFormat getPixFmt() const;

    private:
        std::string *filename;
        const AVCodec *codec;
        AVCodecParserContext *parser;
        AVCodecContext *c= nullptr;
        AVFrame *frame;
        uint8_t *data;
        AVPacket *pkt;

 };

