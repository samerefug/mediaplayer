#include "player/BaseDecoder.hpp"



BaseDecoder::BaseDecoder(): codec(nullptr),c(nullptr),parser(nullptr){}
BaseDecoder::~BaseDecoder() {
    avcodec_free_context(&c);
    av_parser_close(parser);
    //maybe add delete queue code
}


bool BaseDecoder::initializeById(AVCodecID codec_id){

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
    return true;
}

bool BaseDecoder::initializeByStream(AVStream* stream){
    AVCodecParameters *codecpar = stream->codecpar;
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
    return true;
}


bool BaseDecoder::parsePacket(uint8_t*& data, size_t& data_size,AVPacket* pkt) 
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

bool BaseDecoder::decodePacket(AVPacket* pkt){
    int ret = avcodec_send_packet(c, pkt);
    if(ret<0){
        fprintf(stderr, "Error sending a packet for decoding\n");
        return false;
    }
    while (ret >= 0){
        AVFrame* decoded_frame = av_frame_alloc();
        ret = avcodec_receive_frame(c, decoded_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            av_frame_free(&decoded_frame);
            break;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            av_frame_free(&decoded_frame);
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            frame_queue_.push(decoded_frame);
        }
    }
    return true;
}

bool BaseDecoder::flush(){
    return decodePacket(nullptr);
}


AVFrame *BaseDecoder::getDecodedFrame()
{
    std::unique_lock<std::mutex> lock(frame_mutex_);
    if (frame_queue_.empty()) 
        return nullptr;
    AVFrame* decoded_frame = nullptr;
    decoded_frame = frame_queue_.front();
    frame_queue_.pop();
    return decoded_frame;
}