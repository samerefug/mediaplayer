#ifndef BASEDECODER_HPP
#define BASEDECODER_HPP


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
 extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
 }
 #define INBUF_SIZE 4096

 class BaseDecoder {
    public:
      BaseDecoder();
      virtual ~BaseDecoder();
      bool isInitialized() {return isinit;}
      bool initialize_raw(AVCodecID codec_id);
      bool initialize_fromstream(AVCodecParameters* codecpar);
      virtual bool parsePacket(uint8_t *&inbuf, size_t& size,AVPacket* pkt) = 0;
      virtual bool sendPacketAndReceiveFrame(AVPacket* pkt) = 0;
      virtual AVFrame* getFrame() const = 0;
      int set_parameter_bystreams(AVStream* st);
      virtual bool flush() = 0;
      virtual int get_bytes_per_sample() const { return 0; }
      virtual int get_channels() const { return 0; }
      AVCodecID  get_codec_id() const {return codec->id;}
      AVSampleFormat get_sample_fmt();
      int get_sample_rate();
      uint64_t get_ch();
    protected:
        bool isinit;
        const AVCodec *codec;
        AVCodecContext *c= NULL;
        AVCodecParserContext *parser = NULL;
        AVFrame *decoded_frame = NULL;
 };

inline int BaseDecoder::set_parameter_bystreams(AVStream* st){
    int ret = 0;
    if((ret = avcodec_parameters_to_context(c,st->codecpar))<0){
      fprintf(stderr, "Failed to copy codec parameters to decoder context\n");
    }
    return ret;
}
inline AVSampleFormat BaseDecoder::get_sample_fmt(){
    return c->sample_fmt;
}

inline uint64_t BaseDecoder::get_ch(){
    return c->channel_layout;
}


inline int BaseDecoder::get_sample_rate(){
    return c->sample_rate;
}


inline bool BaseDecoder::initialize_raw(AVCodecID codec_id){

    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
 
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
 
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    
    parser = av_parser_init(codec_id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        exit(1);
    }


    decoded_frame = av_frame_alloc();
    if (!decoded_frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return false;
    }

    isinit = true;
    return true;
}

inline bool BaseDecoder::initialize_fromstream(AVCodecParameters* codecpar){
  codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return false;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate codec context\n");
        return false;
    }

    if (avcodec_parameters_to_context(c, codecpar) < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        return false;
    }

    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

    decoded_frame = av_frame_alloc();
    if (!decoded_frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return false;
    }

    isinit = true;
    return true;
}




inline BaseDecoder::BaseDecoder(): codec(nullptr),c(nullptr),parser(nullptr),decoded_frame(nullptr),isinit(false) {}
inline BaseDecoder::~BaseDecoder() {
    avcodec_free_context(&c);
    av_parser_close(parser);
    av_frame_free(&decoded_frame);
}

 #endif