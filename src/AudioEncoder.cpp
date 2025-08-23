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


void AudioEncoder::add_adts_header(uint8_t* adtsHeader, int packetLen) {
    int profile = 2;   // AAC LC
    int freqIdx = 4;   // 44100Hz
    int chanCfg = 2;   // stereo
    int fullLen = packetLen + 7;

    adtsHeader[0] = 0xFF;
    adtsHeader[1] = 0xF1;
    adtsHeader[2] = ((profile-1)<<6) + (freqIdx<<2) + (chanCfg>>2);
    adtsHeader[3] = ((chanCfg & 3)<<6) + (fullLen>>11);
    adtsHeader[4] = (fullLen & 0x7FF)>>3;
    adtsHeader[5] = ((fullLen & 7)<<5) + 0x1F;
    adtsHeader[6] = 0xFC;
}
 bool AudioEncoder::init(const char* name, AVCodecID id, AVDictionary* opt_arg){
    int ret;
    AVDictionary *opt = nullptr;

    if (!name && id == AV_CODEC_ID_NONE) {
        id = AV_CODEC_ID_AAC;  // 默认AAC
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
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
 
    c->bit_rate = 64000;
    c->sample_fmt = AV_SAMPLE_FMT_FLTP;
    if (!check_sample_fmt(codec, c->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(c->sample_fmt));
        exit(1);
    }
 
    c->sample_rate = select_sample_rate(codec);

    ret = select_channel_layout(codec, &c->ch_layout);
    c->channels = c->ch_layout.nb_channels;
    if (ret < 0)
        exit(1);
    
    av_dict_copy(&opt,opt_arg,0);
    if (avcodec_open2(c, codec, &opt) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    printf("Audio channels: %d\n", c->channels);
    return true;
 }

 void AudioEncoder::close(){
    if (!c) 
        return;
    if (c) {
        avcodec_free_context(&c);
        c = nullptr;
    }
 }

int AudioEncoder::encode(AVFrame* encode_frame, PacketCallback callback){
    int ret;
    ret = avcodec_send_frame(c, encode_frame);
    if(!encode_frame) { 
        fprintf(stderr, "flush\n");
    }
    if (ret < 0) {
        if (encode_frame) {
            fprintf(stderr, "Error sending frame to encoder\n");
        } else {
            fprintf(stderr, "Error flushing encoder audio\n");
        }
        exit(1);
    }
    while(ret >=0){
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
        //本地播放aac添加adts头
        // uint8_t adtsHeader[7];
        // add_adts_header(adtsHeader, pkt->size);

        // uint8_t* buffer = new uint8_t[pkt->size + 7];
        // memcpy(buffer, adtsHeader, 7);
        // memcpy(buffer + 7, pkt->data, pkt->size);
        // pkt->data = buffer;
        // pkt->size = pkt->size + 7;
        callback(pkt);
        av_packet_free(&pkt);
    }
    return ret;
}