#include "videoDecoder.hpp"
#include <iostream>
videoDecoder::videoDecoder()
    : codec(nullptr), parser(nullptr), c(nullptr), frame(nullptr), pkt(nullptr)
{
}

videoDecoder::~videoDecoder(){
    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

bool videoDecoder::initialize(AVCodecID codec_id){

    codec = avcodec_find_decoder(codec_id);
    if(!codec){
        std::cerr << "Codec not found" << std::endl;
        return false;
    }
    
    parser = av_parser_init(codec->id);
    if (!parser) {
        std::cerr << "Parser not found" << std::endl;
        return false;
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }
    if (avcodec_open2(c, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return false;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "Could not allocate AVPacket" << std::endl;
        return false;
    }
    frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return false;
    }
    isinit = true;
    return true;
}

bool videoDecoder::loadPacket(uint8_t*& data, size_t& data_size) 
{   
    int ret = 0;
    ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                            data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0) {
        std::cerr << "Error while parsing" << std::endl;
        return false;
    }
    data += ret;
    data_size -= ret;
    return true;
}

bool videoDecoder::sendPacketAndReceiveFrame() {
    //只有pkt>0才表示有一帧数据解析完成
    if (pkt->size <= 0) {
        return false;
    }
    int ret;

    ret = avcodec_send_packet(c, pkt);
    if (ret < 0) {
        std::cerr << "Error sending packet for decoding\n";
        exit(1);
    }
 
    while (ret >= 0) {
        ret = avcodec_receive_frame(c, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            printf("false");
            return false;
        }
        else if (ret < 0) {
            std::cerr << "Error during decoding" << std::endl;
            exit(1);
        }
        return true;
    }
    return true;
}

bool videoDecoder::flush(){
    int ret = avcodec_send_packet(c, nullptr);
    if (ret < 0) {
        std::cerr << "Error sending flush packet for decoding" << std::endl;
        return false;
    }
    ret = avcodec_receive_frame(c, frame);
    if (ret< 0 ) return false;
    return true;
}

AVFrame* videoDecoder::getFrame() const {
     if (frame && frame->width > 0 && frame->height > 0) {
        return frame;
    }
    return nullptr;
}

int videoDecoder::getWidth() const {
    return frame ? frame->width : 0;
}

int videoDecoder::getHeight() const {

    return frame ? frame->height : 0;
}

AVPixelFormat videoDecoder::getPixFmt() const {
    return frame ? static_cast<AVPixelFormat>(frame->format) : AV_PIX_FMT_NONE;
}
