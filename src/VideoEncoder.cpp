#include "VideoEncoder.hpp"


bool VideoEncoder::init(const char* name, AVCodecID id,AVDictionary* opt_arg){
    int ret;
    if (!name && id == AV_CODEC_ID_NONE) {
        id = AV_CODEC_ID_H264;  // 默认H264
    }

    if(name){
        codec = avcodec_find_encoder_by_name(name);
        if (!codec) { 
            fprintf(stderr, "Codec '%s' not found\n", name); 
            exit(1); 
        }
    }else if(id != AV_CODEC_ID_NONE){
        codec = avcodec_find_encoder(id);
        if (!codec) {
            fprintf(stderr, "Codec id '%d' not found\n", id);
            exit(1);
        }
    }

    c = avcodec_alloc_context3(codec);
    if (!c) { 
        fprintf(stderr, "Could not allocate video codec context\n"); 
        exit(1); 
    }

    //pkt放置在外部
    // pkt = av_packet_alloc(); if (!pkt) exit(1);

    c->bit_rate = 400000;
    c->width = 960;
    c->height = 400;

    c->time_base = (AVRational){1,25};
    c->framerate = (AVRational){25,1};

    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264) 
        av_opt_set(c->priv_data, "preset", "slow", 0);
    
    ret = avcodec_open2(c, codec, NULL); 
    if (ret < 0) { 
        fprintf(stderr, "Could not open codec"); 
        exit(1); 
    }

    pts = 0;
    return true;
}

 int VideoEncoder::encode(AVFrame* encode_frame, PacketCallback callback) {
    int ret;
    ret = avcodec_send_frame(c, encode_frame);
    if(!encode_frame) { 
        fprintf(stderr, "flush\n");
    }
    if (ret < 0) {
        if (encode_frame) {
            fprintf(stderr, "Error sending frame to encoder\n");
        } else {
            fprintf(stderr, "Error flushing encoder video\n");
        }
        exit(1);
    }
    while (ret >= 0)
    {
        AVPacket* pkt = av_packet_alloc();
        ret = avcodec_receive_packet(c, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;  
        } else if (ret < 0) {
            fprintf(stderr, "Error receiving packet\n");
            av_packet_free(&pkt);
            exit(1);
        }
        callback(pkt);
        av_packet_free(&pkt);
    }
    return ret;
}


void VideoEncoder::close() {
    if (c) {
        avcodec_send_frame(c, nullptr);
        avcodec_free_context(&c);
    }
}
