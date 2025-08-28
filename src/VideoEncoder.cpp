#include "VideoEncoder.hpp"


bool VideoEncoder::init(const char* name, AVCodecID id,AVDictionary* opt_arg){
    int ret;
    if (c) {
        fprintf(stderr, "VideoEncoder already initialized\n");
        return false;
    }
    if (!name && id == AV_CODEC_ID_NONE) {
        id = AV_CODEC_ID_H264;  // 默认H264
    }

    if(name){
        codec = avcodec_find_encoder_by_name(name);
        if (!codec) { 
            fprintf(stderr, "Codec '%s' not found\n", name); 
            return false;
        }
    }else if(id != AV_CODEC_ID_NONE){
        codec = avcodec_find_encoder(id);
        if (!codec) {
            fprintf(stderr, "Codec id '%d' not found\n", id);
            return false;
        }
    }

    c = avcodec_alloc_context3(codec);
    if (!c) { 
        fprintf(stderr, "Could not allocate video codec context\n"); 
        return false;
    }


    c->width = width_;
    c->height = height_;

    c->time_base = (AVRational){1,fps_};
    c->framerate = (AVRational){fps_,1};

    c->pix_fmt = target_pix_fmt_;
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //crf和cbr视频测试
    if(crf_ >=0 && codec->id ==AV_CODEC_ID_H264){
        av_opt_set_int(c->priv_data ,"crf",crf_,0);
        av_opt_set(c->priv_data,"preset",preset_.c_str(),0);
    }else{
        c->bit_rate = bit_rate_;
        c->gop_size = fps_;
        c->max_b_frames = 1;
        if(codec->id == AV_CODEC_ID_H264){
             av_opt_set(c->priv_data, "preset", preset_.c_str(), 0);
        }
    }

    //设定参数打开编码器
    AVDictionary* opts = nullptr;
    if (opt_arg) {
        av_dict_copy(&opts, opt_arg, 0);
    }
    
    ret = avcodec_open2(c, codec, &opts); 
    av_dict_free(&opts);
    if (ret < 0) { 
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Could not open video codec: %s\n", errbuf);
        avcodec_free_context(&c);
        return false;
    }
    printf("Video encoder initialized: %dx%d, %d fps, bitrate: %d\n", 
           width_, height_, fps_, bit_rate_);
    return true;
}

 int VideoEncoder::encode(AVFrame* encode_frame, PacketCallback callback) {
    
    int ret;
    ret = avcodec_send_frame(c, encode_frame);
    if(!encode_frame) { 
        fprintf(stderr, "flush\n");
    }
    if (ret < 0) {
        if (!encode_frame) {
            printf("Flushing video encoder\n");
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "Error sending frame to video encoder: %s\n", errbuf);
            return ret;
        }
    }

    while (ret >= 0)
    {
        AVPacket* pkt = av_packet_alloc();
        if(!pkt){
            fprintf(stderr,"Could not allocate packet\n");
            return AVERROR(ENOMEM);
        }
        ret = avcodec_receive_packet(c, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_free(&pkt);
            break;  
        } else if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "Error receiving packet from video encoder: %s\n", errbuf);
            av_packet_free(&pkt);
            return ret;
        }
        callback(pkt);
        av_packet_free(&pkt);
    }
    return ret;
}

void VideoEncoder::setVideoParams(int width, int height, int bit_rate, int fps) {
    if (c) {
        fprintf(stderr, "Cannot change parameters after initialization\n");
        return;
    }
    width_ = width;
    height_ = height;
    bit_rate_ = bit_rate;
    fps_ = fps;
}

void VideoEncoder::setQuality(const std::string& preset, int crf) {
    if (c) {
        fprintf(stderr, "Cannot change quality settings after initialization\n");
        return;
    }
    preset_ = preset;
    crf_ = crf;
}
void VideoEncoder::close() {
    if (c) {
        avcodec_send_frame(c, nullptr);
        avcodec_free_context(&c);
    }
}
