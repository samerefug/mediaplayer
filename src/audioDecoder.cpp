#include "audioDecoder.hpp"
#include <iostream>


AudioDecoder::AudioDecoder():
    codec(nullptr),c(nullptr),parser(nullptr),pkt(nullptr),decoded_frame(nullptr),sfmt(AV_SAMPLE_FMT_NONE)
{

}

AudioDecoder::~AudioDecoder()
{
    avcodec_free_context(&c);
    av_parser_close(parser);
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);
}

bool AudioDecoder::initialize(AVCodecID codec_id)
{
    pkt = av_packet_alloc();
 
    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
 
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
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
    isinit = true;
    return true;
}

bool AudioDecoder::loadPacket(uint8_t *&data, size_t& size){
    int ret = 0;
    if(!decoded_frame){
        if(!(decoded_frame = av_frame_alloc())){
            fprintf(stderr,"could not allocate audio frame");
            exit(1);
        }
    }
    ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                            data, size,
                            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    data +=ret;
    size -= ret;
    return pkt->size;
}

bool AudioDecoder::sendPacketAndReceiveFrame()
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
    pkt->data = nullptr;
    pkt->size = 0;
    sendPacketAndReceiveFrame();
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