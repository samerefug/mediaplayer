#include "LiveStreamer.hpp"

LiverStreamer::LiverStreamer(){

}

LiverStreamer::~LiverStreamer(){
    stop();
}

bool LiverStreamer::configure(const Config& config){
    config_  = config;
    if(config_.audio_file.empty() || config_.video_file.empty()){
        fprintf(stderr,"audio or video file not specified");
        return false;
    }

    if(config_.rtmp_url.empty()){
        fprintf(stderr,"RTMP URL not specifed");
        return false;
    }

    return initializeComponents();
}

bool LiverStreamer::initializeComponents(){
    int ret;
    data_manager_ = std::make_unique<MediaDataManager>();
    data_manager_->initAudioBuffer(config_.audio_sample_rate,config_.audio_channels,AV_SAMPLE_FMT_S16);
    data_manager_->initVideoBuffer(config_.video_width,config_.video_height,AV_PIX_FMT_YUV420P);

    formatConverter_ = std::make_unique<MediaFormatConverter>();
   

    audio_source_ = std::make_unique<RawFileDataSource>(config_.audio_file,RawFileDataSource::FileType::PCM_FILE);
    audio_source_->setPCMParams(config_.audio_sample_rate,config_.audio_channels,AV_SAMPLE_FMT_S16,1024);
    audio_source_->setDataManager(data_manager_.get());
    audio_source_->setFormatConverter(formatConverter_.get());


    video_source_ = std::make_unique<RawFileDataSource>(config_.video_file,RawFileDataSource::FileType::YUV_FILE);
    video_source_->setYUVParams(config_.video_width ,config_.video_height ,AV_PIX_FMT_YUV420P,config_.video_fps);
    video_source_->setDataManager(data_manager_.get());
    video_source_->setFormatConverter(formatConverter_.get());
    audio_encoder_ = std::make_unique<AudioEncoder>();
    audio_encoder_->setAudioParams(config_.audio_sample_rate, config_.audio_channels, config_.audio_bitrate);
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


    muxer_->addAudioStream(audio_encoder_->getCodecParameters());
    muxer_->addVideoStream(video_encoder_->getCodecParameters());

    coordinator_ = std::make_unique<EncodingCoordinator>();
    coordinator_->setDataManager(data_manager_.get());
    coordinator_->setAudioEncoder(audio_encoder_.get());
    coordinator_->setVideoEncoder(video_encoder_.get());
    coordinator_->setMuxer(muxer_.get());
    //音视频时间戳同步
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
    if(audio_source_) audio_source_->stop();
    if(video_source_) video_source_->stop();

    if(coordinator_) coordinator_->stop();
    if(muxer_) muxer_->finalize();

    if(publisher_) publisher_->stop();
}