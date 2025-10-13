#include "streamer/LiveStreamer.hpp"


static AVSampleFormat get_default_sample_fmt(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_MP2:
            return AV_SAMPLE_FMT_S16;
        case AV_CODEC_ID_MP3:
            return AV_SAMPLE_FMT_FLTP;
        case AV_CODEC_ID_AAC:
            return AV_SAMPLE_FMT_FLTP;
        case AV_CODEC_ID_AC3:
            return AV_SAMPLE_FMT_FLTP;
        case AV_CODEC_ID_FLAC:
            return AV_SAMPLE_FMT_S32;
        case AV_CODEC_ID_OPUS:
            return AV_SAMPLE_FMT_FLT;
        default:
            return AV_SAMPLE_FMT_S16;
    }
}

LiverStreamer::LiverStreamer():stopped_(false){

}

LiverStreamer::~LiverStreamer(){
    if (!stopped_) { 
        stop();
    }
}

bool LiverStreamer::configure(const Config& config){


    config_ = config; 

    if (config_.audio_sample_rate <= 0) {
        config_.audio_sample_rate = 44100;
        printf("[Config] audio_sample_rate not set, using default: %d\n", config_.audio_sample_rate);
    }

    if (config_.audio_channels <= 0) {
        config_.audio_channels = 2;
        printf("[Config] audio_channels not set, using default: %d\n", config_.audio_channels);
    }

    if (config_.audio_bitrate <= 0) {
        config_.audio_bitrate = 128000;
        printf("[Config] audio_bitrate not set, using default: %d\n", config_.audio_bitrate);
    }

    if(config_.audio_fmt == AV_SAMPLE_FMT_NONE){
        config_.audio_fmt = AV_SAMPLE_FMT_FLTP;
        printf("[Config] audio_fmt not set, using default:S16\n");
    }

    if (config_.audio_codec == AV_CODEC_ID_NONE) {
        config_.audio_codec = AV_CODEC_ID_AAC;
        printf("[Config] audio_codec not set, using default:AAC\n");
    }

    if (config_.video_width <= 0) {
        config_.video_width = 960;
        printf("[Config] video_width not set, using default: %d\n", config_.video_width);
    }

    if (config_.video_height <= 0) {
        config_.video_height = 400;
        printf("[Config] video_height not set, using default: %d\n", config_.video_height);
    }

    if (config_.video_fps <= 0) {
        config_.video_fps = 25;
        printf("[Config] video_fps not set, using default: %d\n", config_.video_fps);
    }

    if (config_.video_bitrate <= 0) {
        config_.video_bitrate = 200000;
        printf("[Config] video_bitrate not set, using default: %d\n", config_.video_bitrate);
    }

    
    if(config_.video_fmt == AV_PIX_FMT_NONE){
        config_.video_fmt = AV_PIX_FMT_YUV420P;
        printf("[Config] video_fmt not set, using default:YUV420\n");
    }

    if (config_.video_codec == AV_CODEC_ID_NONE) {
        config_.video_codec = AV_CODEC_ID_H264;
        printf("[Config] video_codec not set, using default: H264\n");
    }

    if (config_.output_format.empty()) {
        config_.output_format = "mp4";
        printf("[Config] output_format not set, using default: mp4\n");
    }

    if (config_.audio_file.empty()) {
        printf("[Config Warning] audio_file not set! No audio will be streamed.\n");
        return false;
    }
    if (config_.video_file.empty()) {
        printf("[Config Warning] video_file not set! No video will be streamed.\n");
        return false;
    }
    if (config_.rtmp_url.empty()) {
        printf("[Config Warning] rtmp_url not set! Output will not be streamed.\n");
        return false;
    }
    return initializeComponents();
}


bool LiverStreamer::initializeComponents(){
    int ret;
    data_manager_ = std::make_unique<MediaDataManager>();
    //Buffer format setup, needs to be converted to a format supported by the encoder
    data_manager_->initAudioBuffer(config_.audio_sample_rate,config_.audio_channels,get_default_sample_fmt(config_.audio_codec));
    data_manager_->initVideoBuffer(config_.video_width,config_.video_height,config_.video_fmt);

    audio_formatConverter_ = std::make_unique<MediaFormatConverter>();
    // Data source format, manually specified
    audio_source_ = std::make_unique<RawFileDataSource>(config_.audio_file,RawFileDataSource::FileType::PCM_FILE);
    audio_source_->setPCMParams(config_.audio_sample_rate,config_.audio_channels,config_.audio_fmt,1024);
    audio_source_->setDataManager(data_manager_.get());
    audio_source_->setFormatConverter(audio_formatConverter_.get());

    video_source_ = std::make_unique<RawFileDataSource>(config_.video_file,RawFileDataSource::FileType::YUV_FILE);
    video_source_->setYUVParams(config_.video_width ,config_.video_height ,AV_PIX_FMT_YUV420P,config_.video_fps);
    video_source_->setDataManager(data_manager_.get());


    audio_encoder_ = std::make_unique<AudioEncoder>();
    audio_encoder_->setAudioParams(config_.audio_sample_rate, config_.audio_channels, config_.audio_bitrate);
    audio_encoder_->setSampleFormat(get_default_sample_fmt(config_.audio_codec));

    if(!audio_encoder_->init(nullptr,config_.audio_codec,nullptr)){
        fprintf(stderr,"failed to init audio encoder");
        return false;
    }

    video_encoder_ = std::make_unique<VideoEncoder>();
    video_encoder_->setVideoParams(config_.video_width,config_.video_height,config_.video_bitrate,config_.video_fps);
    if(!video_encoder_->init(nullptr,config_.video_codec,nullptr)){
        fprintf(stderr,"failed to init video encoder");
        return false;
    }

    muxer_ = std::make_unique<AVMuxer>(config_.rtmp_url,config_.output_format);
    if(!muxer_->init()){
        fprintf(stderr,"failed to initialize muxer");
        return false;
    }

    AVChannelLayout src_layout;
    av_channel_layout_default(&src_layout, config_.audio_channels);

    AVChannelLayout dst_layout;
    av_channel_layout_default(&dst_layout, config_.audio_channels);

    audio_formatConverter_->initAudioConverter(config_.audio_fmt,config_.audio_sample_rate,src_layout,
                                            get_default_sample_fmt(config_.audio_codec), config_.audio_sample_rate ,dst_layout );

    muxer_->addAudioStream(audio_encoder_->getCodecParameters());
    muxer_->addVideoStream(video_encoder_->getCodecParameters());

    coordinator_ = std::make_unique<EncodingCoordinator>();
    coordinator_->setDataManager(data_manager_.get());
    coordinator_->setAudioEncoder(audio_encoder_.get());
    coordinator_->setVideoEncoder(video_encoder_.get());
    coordinator_->setMuxer(muxer_.get());
    coordinator_->setAudioVideoSync(config_.enable_av_sync);

    publisher_=std::make_unique<StreamPublisher>();
    publisher_->configure(config_.rtmp_url);
    publisher_->setMuxer(muxer_.get());
    muxer_->setStreamPublisher(publisher_.get());

    return true;
}

bool LiverStreamer::start(){
    if(!muxer_->writeHeader()){
        fprintf(stderr,"failed to write muxer header");
        return false;
    }

    if(!audio_source_->open() || !audio_source_->start()){
        fprintf(stderr,"failed to start audio source");
        return false;
    }

    if(!video_source_->open() || !video_source_->start()){
        fprintf(stderr,"failed to start video source");
        return false;
    }

    if(!publisher_->start()){
        fprintf(stderr,"failed to start publisher");
        return false;
    }

    if(!coordinator_->start()){
        fprintf(stderr,"failed to start encoding coordinator");
        return false;
    }
    return true;
}

void LiverStreamer::stop(){
    if (stopped_) return; 
    if (audio_source_) audio_source_->stop();
    if (video_source_) video_source_->stop();
    if (coordinator_) coordinator_->stop();
    if (muxer_) muxer_->finalize();
    if (publisher_) publisher_->stop();
    stopped_ = true;
}