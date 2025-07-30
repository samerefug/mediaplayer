#include "videoDecoder.hpp"
#include <iostream>
videoDecoder::videoDecoder(){}

videoDecoder::~videoDecoder(){}


//仅在裸流处理部分需要解析
bool videoDecoder::parsePacket(uint8_t*& data, size_t& data_size,AVPacket* pkt) 
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

bool videoDecoder::sendPacketAndReceiveFrame(AVPacket* pkt) {
    int ret;
    ret = avcodec_send_packet(c, pkt);
    if (ret < 0) {
        std::cerr << "Error sending packet for decoding\n";
        exit(1);
    }
 
    while (ret >= 0) {
        ret = avcodec_receive_frame(c, decoded_frame);
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
    ret = avcodec_receive_frame(c, decoded_frame);
    if (ret< 0 ) return false;
    return true;
}

AVFrame* videoDecoder::getFrame() const {
     if (decoded_frame && decoded_frame->width > 0 && decoded_frame->height > 0) {
        return decoded_frame;
    }
    return nullptr;
}

int videoDecoder::getWidth() const {
    return decoded_frame ? decoded_frame->width : 0;
}

int videoDecoder::getHeight() const {

    return decoded_frame ? decoded_frame->height : 0;
}

AVPixelFormat videoDecoder::getPixFmt() const {
    return decoded_frame ? static_cast<AVPixelFormat>(decoded_frame->format) : AV_PIX_FMT_NONE;
}
