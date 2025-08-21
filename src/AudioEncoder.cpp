#include "AudioEncoder.hpp"


static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;
 
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

static int select_sample_rate(const AVCodec *codec)
{
    const int *p;
    int best_samplerate = 0;
 
    if (!codec->supported_samplerates)
        return 44100;
 
    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

static int select_channel_layout(const AVCodec *codec, AVChannelLayout *dst)
{
    const AVChannelLayout *p, *best_ch_layout;
    int best_nb_channels   = 0;
     AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    if (!codec->ch_layouts)
        return av_channel_layout_copy(dst, &stereo);
 
    p = codec->ch_layouts;
    while (p->nb_channels) {
        int nb_channels = p->nb_channels;
 
        if (nb_channels > best_nb_channels) {
            best_ch_layout   = p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return av_channel_layout_copy(dst, best_ch_layout);
}

 bool AudioEncoder::init(){
    int ret;
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
 
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
 
    c->bit_rate = 64000;
 
    c->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, c->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(c->sample_fmt));
        exit(1);
    }
 
    c->sample_rate = select_sample_rate(codec);
    ret = select_channel_layout(codec, &c->ch_layout);
    if (ret < 0)
        exit(1);
 
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    encode_frame = av_frame_alloc();
    if (!encode_frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }
 
    encode_frame->nb_samples = c->frame_size;
    encode_frame->format     = c->sample_fmt;
    ret = av_channel_layout_copy(&encode_frame->ch_layout, &c->ch_layout);
    if (ret < 0)
        exit(1);
 
    ret = av_frame_get_buffer(encode_frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        exit(1);
    }
    return true;
 }

 void AudioEncoder::close(){
    if (!c) 
        return;
    if (encode_frame) {
        av_frame_free(&encode_frame);
        encode_frame = nullptr;
    }
    if (c) {
        avcodec_free_context(&c);
        c = nullptr;
    }
 }

 AVPacket* AudioEncoder::encode(uint8_t* pcm_data,AVPacket*pkt){
    int ret;

    if (!pcm_data) {
        ret = avcodec_send_frame(c, nullptr); 
        if (ret < 0) {
            fprintf(stderr, "Error sending flush frame\n");
            return nullptr;
        }

        ret = avcodec_receive_packet(c, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return nullptr;
        } else if (ret < 0) {
            fprintf(stderr, "Error receiving packet during flush\n");
            return nullptr;
        }
        return pkt; 
    }   

    ret = av_frame_make_writable(encode_frame);
    if (ret < 0) {
        fprintf(stderr, "Frame not writable\n");
        return nullptr;
    }

    int channels = encode_frame->ch_layout.nb_channels;
    int bytes_per_sample = av_get_bytes_per_sample((AVSampleFormat)encode_frame->format);

    memcpy(encode_frame->data[0], pcm_data, encode_frame->nb_samples * channels * bytes_per_sample);
    ret = avcodec_send_frame(c, encode_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        return nullptr;
    }

    ret = avcodec_receive_packet(c, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;  
    } else if (ret < 0) {
        fprintf(stderr, "Error encoding audio frame\n");
        return nullptr;
    }
    return pkt;
 }