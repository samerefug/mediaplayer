#ifndef AUDIOENCODER
#define AUDIOENCODER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <functional>
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/common.h>
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
}

#include "BaseEncoder.hpp"


class AudioEncoder :public BaseEncoder{
public:
    AudioEncoder(){};
    ~AudioEncoder(){close();}
    bool init(const char* name = nullptr, AVCodecID id = AV_CODEC_ID_NONE ,AVDictionary* opt_arg =nullptr) override;
    void close() override;
    int encode(AVFrame* encode_frame) override;
    int getFrameSize() const;
    void setAudioParams(int sample_rate = 44100,int channels = 2,int bit_rate = 128000);
    void setSampleFormat(AVSampleFormat fmt){target_sample_fmt_ = fmt;}

private:
    static int check_sample_fmt(const AVCodec* codec,enum AVSampleFormat sample_fmt);
    static int select_sample_rate(const AVCodec* codec,int preferred_rate);
    static int select_channel_layout(const AVCodec* codec,AVChannelLayout* dst,int preferred_channels);
    void add_adts_header(uint8_t* adtsHeader, int packetLen);

private:
    int sample_rate_ = 44100;
    int channels_ = 2;
    int bit_rate_ = 128000;
    AVSampleFormat target_sample_fmt_;

};

#endif