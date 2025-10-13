#include "AudioDecoder.hpp"
#include <iostream>


AudioDecoder::AudioDecoder()
{

}

AudioDecoder::~AudioDecoder()
{
    avcodec_free_context(&c);
}

int AudioDecoder::getBytes_per_sample() const
{
    int data_size;
    data_size = av_get_bytes_per_sample(c->sample_fmt);
    if(data_size < 0){
        fprintf(stderr, "Failed to calculate data size\n");
        exit(1);
    }
    return data_size;
}

int AudioDecoder::getChannels() const
{
     return c ? c->ch_layout.nb_channels : 0;
}

int AudioDecoder::getSampleRate() const
{
    return c ? c->sample_rate : 0;
}

AVSampleFormat AudioDecoder::getSampleFormat() const {
    return c? c->sample_fmt : AV_SAMPLE_FMT_NONE;
}

AVRational AudioDecoder::getTimeBase() const {
    if (!c) {
        return {1, 48000}; // 默认48000Hz
    }
    return c->time_base;
}