#include "audioDecoder.hpp"
#include <iostream>


AudioDecoder::AudioDecoder():
   sfmt(AV_SAMPLE_FMT_NONE)
{

}

AudioDecoder::~AudioDecoder()
{
    avcodec_free_context(&c);
}

bool AudioDecoder::parsePacket(uint8_t *&data, size_t& size,AVPacket* pkt){
    int ret = 0;
    ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                            data, size,
                            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    data +=ret;
    size -= ret;
    return true;
}



bool AudioDecoder::sendPacketAndReceiveFrame(AVPacket* pkt)
{
    int i ,ch;
    int ret,data_size;
    //pkt数据送入解码器
    ret = avcodec_send_packet(c, pkt);
    if(ret<0){
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    //接收解码后的帧
    while (ret >= 0){
        ret = avcodec_receive_frame(c, decoded_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return false;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        return true;
    }
    return true;
}


bool AudioDecoder::flush(){
    int ret = avcodec_send_packet(c, nullptr);
    if (ret < 0) {
        std::cerr << "Error sending flush packet for decoding" << std::endl;
        return false;
    }
    ret = avcodec_receive_frame(c, decoded_frame);
    if (ret< 0 ) return false;
    return true;
}


AVFrame *AudioDecoder::getFrame() const
{
    return decoded_frame;
}

int AudioDecoder::get_bytes_per_sample() const
{
    int data_size;
    data_size = av_get_bytes_per_sample(c->sample_fmt);
    if(data_size < 0){
        fprintf(stderr, "Failed to calculate data size\n");
        exit(1);
    }
    return data_size;
}

int AudioDecoder::get_channels() const
{
    return c->channels;
}

