#ifndef BASEDECODER_HPP
#define BASEDECODER_HPP


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <queue>
#include <mutex>
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
 }
 #define INBUF_SIZE 4096

 class BaseDecoder {
    public:
        BaseDecoder();
        virtual ~BaseDecoder();

        bool initializeById(AVCodecID codec_id);
        bool initializeByStream(AVStream* stream);
        //parser should remove
        virtual bool parsePacket(uint8_t *&inbuf, size_t& size,AVPacket* pkt);
        virtual bool decodePacket(AVPacket* packet);
        virtual AVFrame* getDecodedFrame();
        virtual bool flush();
    protected:
        const AVCodec *codec;
        AVCodecContext *c;
        AVCodecParserContext *parser;
        std::queue<AVFrame*> frame_queue_;
        std::mutex frame_mutex_;
};
 #endif