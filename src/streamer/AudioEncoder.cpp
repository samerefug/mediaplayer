#include "streamer/AudioEncoder.hpp"


int AudioEncoder::check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;
 
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

int AudioEncoder::select_sample_rate(const AVCodec *codec,int preferred_rate)
{
    const int *p;
    int best_samplerate = preferred_rate;
 
    if (!codec->supported_samplerates)
        return 44100;
 
    p = codec->supported_samplerates;
    while (*p) {
        if (abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}


int AudioEncoder::select_channel_layout(const AVCodec* codec, AVChannelLayout* dst, int preferred_channels) {
    const AVChannelLayout* p;
    const AVChannelLayout* best_ch_layout = nullptr;
    int best_nb_channels = 0;

    // 根据偏好声道数选择布局
    if (preferred_channels == 1) {
        AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
        return av_channel_layout_copy(dst, &mono);
    } else if (preferred_channels == 2) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        return av_channel_layout_copy(dst, &stereo);
    }

    if (!codec->ch_layouts) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        return av_channel_layout_copy(dst, &stereo);
    }

    p = codec->ch_layouts;
    while (p && p->nb_channels) {
        if (abs(p->nb_channels - preferred_channels) < abs(best_nb_channels - preferred_channels)) {
            best_ch_layout = p;
            best_nb_channels = p->nb_channels;
        }
        p++;
    }

    if (best_ch_layout) {
        return av_channel_layout_copy(dst, best_ch_layout);
    }

    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    return av_channel_layout_copy(dst, &stereo);
}

void AudioEncoder::setAudioParams(int sample_rate, int channels, int bit_rate) {
    if (c) {
        fprintf(stderr, "Cannot change parameters after initialization\n");
        return;
    }
    sample_rate_ = sample_rate;
    channels_ = channels;
    bit_rate_ = bit_rate;
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
    if(c){
        fprintf(stderr,"Audio already initialized\n");
        return false;
    }
    AVDictionary *opt = nullptr;
    if (!name && id == AV_CODEC_ID_NONE) {
        id = AV_CODEC_ID_AAC;  // 默认AAC
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
        fprintf(stderr, "Could not allocate audio codec context\n");
        return false;
    }
 
    c->bit_rate = bit_rate_;
    c->sample_fmt = target_sample_fmt_;
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (!check_sample_fmt(codec, c->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(c->sample_fmt));
        return false;
    }
 
    c->sample_rate = select_sample_rate(codec,sample_rate_);
   
    int ret = select_channel_layout(codec, &c->ch_layout,channels_);
    if(ret < 0){
        fprintf(stderr,"Could not select channel layout \n");
        avcodec_free_context(&c);
        return false;
    }
    c->channels = c->ch_layout.nb_channels;

    
    AVDictionary* opts = nullptr;
    if (opt_arg) {
        av_dict_copy(&opts, opt_arg, 0);
    }
    if (avcodec_open2(c, codec, &opts) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }
    av_dict_free(&opts);
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
int AudioEncoder::getFrameSize() const{
    if (!c) {
        fprintf(stderr, "AudioEncoder::getFrameSize called before encoder init\n");
        return 0;
    }
    return c->frame_size;
}
int AudioEncoder::encode(AVFrame* encode_frame){
    int ret;
    fprintf(stderr, "frame=%p nb_samples=%d format=%d sample_rate=%d channels=%d\n",
        encode_frame,
        encode_frame ? encode_frame->nb_samples : -1,
        encode_frame ? encode_frame->format : -1,
        encode_frame ? encode_frame->sample_rate : -1,
        encode_frame ? encode_frame->channels : -1);
    fprintf(stderr, "ctx=%p codec=%p fmt=%d rate=%d channels=%d\n",
        c, c ? c->codec : nullptr,
        c ? c->sample_fmt : -1,
        c ? c->sample_rate : -1,
        c ? c->channels : -1);
    ret = avcodec_send_frame(c, encode_frame);
    if(!encode_frame) { 
        fprintf(stderr, "audio flush\n");
    }
    if (ret < 0) {
        if (encode_frame) {
            fprintf(stderr, "Error sending frame to encoder\n");
        } else {
            fprintf(stderr, "Error flushing encoder audio\n");
        }
       return ret ;
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
        {
            std::lock_guard<std::mutex> lock(packet_mutex);
            packet_queue_.push(pkt);
        }
    }
    return ret;
}