#include "FormatConverter.hpp"

MediaFormatConverter::MediaFormatConverter()
    : sws_ctx_(nullptr)
    , converted_video_frame_(nullptr)
    , video_converter_initialized_(false)
    , swr_ctx_(nullptr)
    , converted_audio_frame_(nullptr)
    , audio_converter_initialized_(false)
    , dst_video_width_(0), dst_video_height_(0)
    , dst_video_format_(AV_PIX_FMT_NONE)
    , dst_audio_format_(AV_SAMPLE_FMT_NONE)
    , dst_audio_sample_rate_(0)
    , max_dst_samples_(0) {
    
}

MediaFormatConverter::~MediaFormatConverter() {
    cleanup();
}

bool MediaFormatConverter::initVideoConverter(int src_width, int src_height, AVPixelFormat src_format,
                                             int dst_width, int dst_height, AVPixelFormat dst_format) {
    
    if(src_width == dst_width && src_height == dst_height || src_format == dst_format){
        printf("don't need convert");
        return true;
    }
    
    //创建转换上下文
    if(sws_ctx_){
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    sws_ctx_ = sws_getContext(
    src_width, src_height, src_format,      
    dst_width, dst_height, dst_format,      
    SWS_BICUBIC, nullptr, nullptr, nullptr 
    );

    if(!sws_ctx_){
        fprintf(stderr,"can not create convert");
        return false;
    }
    dst_video_width_ = dst_width;
    dst_video_height_ = dst_height;
    dst_video_format_ = dst_format;
    if(allocateVideoFrame()){
        return false;
    }
    video_converter_initialized_ = true;
    printf("视频转换器初始化成功: %dx%d %s -> %dx%d %s\n",
           src_width, src_height, av_get_pix_fmt_name(src_format),
           dst_width, dst_height, av_get_pix_fmt_name(dst_format));
    return true;
}

bool MediaFormatConverter::allocateVideoFrame() {
    if(converted_video_frame_){
        av_frame_free(&converted_video_frame_);
    }

    converted_video_frame_ = av_frame_alloc();
    if(!converted_video_frame_){
        fprintf(stderr,"can not alloc frame");
        return false;
    }

    converted_video_frame_->width = dst_video_width_;
    converted_video_frame_->height = dst_video_height_;
    converted_video_frame_->format = dst_video_format_;

    int ret = av_frame_get_buffer(converted_video_frame_,0);
    if(ret < 0){
        fprintf(stderr,"can not alloc frame buffer");
        return false;
    }
    return true;
}

AVFrame* MediaFormatConverter::convertVideo(AVFrame* src_frame) {

    if (!video_converter_initialized_ || !sws_ctx_ || !converted_video_frame_) {
        // 无需转换，直接返回源帧的引用
        printf("converter not init, output src_frame");
        return av_frame_clone(src_frame);
    }
    int ret = sws_scale(sws_ctx_,
                        src_frame->data,src_frame->linesize,0,src_frame->height,
                        converted_video_frame_->data,converted_video_frame_->linesize);
    
    if(ret < 0){
        fprintf(stderr,"convert fail");
        return nullptr;
    }

    converted_video_frame_->pts = src_frame->pts;
    converted_video_frame_->pkt_dts = src_frame->pkt_dts;
    converted_video_frame_->pkt_duration = src_frame->pkt_duration;

    return av_frame_clone(converted_video_frame_);
}

bool MediaFormatConverter::initAudioConverter(AVSampleFormat src_format, int src_sample_rate, AVChannelLayout src_layout,
                                             AVSampleFormat dst_format, int dst_sample_rate, AVChannelLayout dst_layout) {
    if(src_format == dst_format && src_sample_rate == dst_sample_rate && av_channel_layout_compare(&src_layout, &dst_layout) == 0){
        printf("don't need convert");
        return true;
    }
    if(swr_ctx_){
        swr_free(&swr_ctx_);
    }
    swr_ctx_ = swr_alloc();
    if(!swr_ctx_){
        fprintf(stderr,"could not alloc swr");
        return false;
    }

    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &src_layout, 0);
    av_opt_set_int(swr_ctx_, "in_sample_rate", src_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", src_format, 0);
    
    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &dst_layout, 0);
    av_opt_set_int(swr_ctx_, "out_sample_rate", dst_sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", dst_format, 0);

    int ret = swr_init(swr_ctx_);
    if (ret < 0) {
        fprintf(stderr, "无法初始化音频重采样器\n");
        swr_free(&swr_ctx_);
        return false;
    }

    dst_audio_format_ = dst_format;
    dst_audio_sample_rate_ = dst_sample_rate;
    av_channel_layout_copy(&dst_audio_layout_, &dst_layout);
    //计算最大输出样本数
    max_dst_samples_ = swr_get_out_samples(swr_ctx_, 4096);
    audio_converter_initialized_ = true;
    printf("音频转换器初始化成功: %s %dHz %dch -> %s %dHz %dch\n",
           av_get_sample_fmt_name(src_format), src_sample_rate, src_layout.nb_channels,
           av_get_sample_fmt_name(dst_format), dst_sample_rate, dst_layout.nb_channels);
    
    return true;
}

bool MediaFormatConverter::allocateAudioFrame(int nb_samples) {
    if (converted_audio_frame_) {
        av_frame_free(&converted_audio_frame_);
    }
    
    converted_audio_frame_ = av_frame_alloc();
    if (!converted_audio_frame_) {
        return false;
    }
    
    converted_audio_frame_->format = dst_audio_format_;
    converted_audio_frame_->sample_rate = dst_audio_sample_rate_;
    converted_audio_frame_->nb_samples = nb_samples;
    av_channel_layout_copy(&converted_audio_frame_->ch_layout, &dst_audio_layout_);
    
    int ret = av_frame_get_buffer(converted_audio_frame_, 0);
    if (ret < 0) {
        av_frame_free(&converted_audio_frame_);
        return false;
    }
    return true;
}

AVFrame* MediaFormatConverter::convertAudio(AVFrame* src_frame) {
if (!audio_converter_initialized_ || !swr_ctx_) {
        return av_frame_clone(src_frame);
    }
    
    int dst_nb_samples = swr_get_out_samples(swr_ctx_, src_frame->nb_samples);
    if (dst_nb_samples < 0) {
        fprintf(stderr, "计算输出样本数失败\n");
        return nullptr;
    }
    
    if (!allocateAudioFrame(dst_nb_samples)) {
        return nullptr;
    }
    
    int converted_samples = swr_convert(swr_ctx_,
                                       converted_audio_frame_->data, dst_nb_samples,
                                       (const uint8_t**)src_frame->data, src_frame->nb_samples);
    
    if (converted_samples < 0) {
        fprintf(stderr, "音频重采样失败\n");
        av_frame_free(&converted_audio_frame_);
        return nullptr;
    }
    
    converted_audio_frame_->nb_samples = converted_samples;
    
    if (src_frame->pts != AV_NOPTS_VALUE) {
        converted_audio_frame_->pts = av_rescale_q(src_frame->pts,
                                                  (AVRational){1, src_frame->sample_rate},
                                                  (AVRational){1, dst_audio_sample_rate_});
    }
    return av_frame_clone(converted_audio_frame_);
}

void MediaFormatConverter::cleanup() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (converted_video_frame_) {
        av_frame_free(&converted_video_frame_);
    }
    
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
    }
    
    if (converted_audio_frame_) {
        av_frame_free(&converted_audio_frame_);
    }
    
    av_channel_layout_uninit(&dst_audio_layout_);
    
    video_converter_initialized_ = false;
    audio_converter_initialized_ = false;
}