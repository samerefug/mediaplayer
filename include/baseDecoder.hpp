#ifndef BASEDECODER_HPP
#define BASEDECODER_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string>
 extern "C"{
    #include <libavcodec/avcodec.h>
 }
 #define INBUF_SIZE 4096

 class BaseDecoder {
    public:
      BaseDecoder();
      virtual ~BaseDecoder();
      bool isInitialized() {return isinit;}
      virtual bool initialize(AVCodecID codec_id) = 0;
      virtual bool loadPacket(uint8_t *&inbuf, size_t& size) = 0;
      virtual bool sendPacketAndReceiveFrame() = 0;
      virtual AVFrame* getFrame() const = 0;
      virtual bool flush() = 0;
      virtual int get_bytes_per_sample() const { return 0; }
      virtual int get_channels() const { return 0; }


    protected:
      bool isinit;
 };

 inline BaseDecoder::BaseDecoder():isinit(false) {}
inline BaseDecoder::~BaseDecoder() {}

 #endif