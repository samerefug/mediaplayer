#include "VideoEncoder.hpp"


bool VideoEncoder::init(){
    int ret;
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) { 
        fprintf(stderr, "Codec '%s' not found\n", codec_name); 
        exit(1); 
    }
    c = avcodec_alloc_context3(codec);
    if (!c) { 
        fprintf(stderr, "Could not allocate video codec context\n"); 
        exit(1); 
    }

    //pkt放置在外部
    // pkt = av_packet_alloc(); if (!pkt) exit(1);

    c->bit_rate = 400000;
    c->width = 352;
    c->height = 288;

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

    encode_frame = av_frame_alloc();
    if (!encode_frame) { 
        fprintf(stderr, "Could not allocate video frame\n"); 
        exit(1); 
    } 
    encode_frame->format = c->pix_fmt; 
    encode_frame->width = c->width; 
    encode_frame->height = c->height; 
    ret = av_frame_get_buffer(encode_frame, 0); 
    if (ret < 0) { 
        fprintf(stderr, "Could not allocate the video frame data\n"); 
        exit(1); 
    }
    pts = 0;
    return true;
}

 AVPacket* VideoEncoder::encode(uint8_t* yuv_data,AVPacket*pkt) {
    // 填充 YUV 数据
    if (!pkt) return nullptr;
    int ret;
    if (yuv_data) {
            // 填充 YUV 数据到 AVFrame
            av_image_fill_arrays(encode_frame->data, encode_frame->linesize, yuv_data,
                                c->pix_fmt, c->width, c->height, 1);
            encode_frame->pts = pts++;

            ret = avcodec_send_frame(c, encode_frame);
            if (ret < 0) {
                fprintf(stderr, "Error sending frame\n");
                exit(1);
            }
        } else {
            // flush 编码器：没有更多帧
            ret = avcodec_send_frame(c, nullptr);
            if (ret < 0) {
                fprintf(stderr, "Error flushing encoder\n");
                return nullptr;
            }
    }

    ret = avcodec_receive_packet(c, pkt);
    if (ret >= 0) {
        return pkt; // 成功编码
    } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr; // 暂时没有可用压缩包
    } else {
        fprintf(stderr, "Error receiving packet");
        return nullptr;
    }
}


void VideoEncoder::close() {
    if (c) {
        avcodec_send_frame(c, nullptr);
        avcodec_free_context(&c);
    }
    if (encode_frame) av_frame_free(&encode_frame);
}
