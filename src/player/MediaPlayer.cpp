#include "MediaPlayer.hpp"

MediaPlayer::MediaPlayer()
    :current_state_(STOPPED)
    ,current_media_url_("")
    {

    demuxer_ = std::make_unique<Demuxer>();
    audio_decoder_ = std::make_unique<AudioDecoder>();
    video_decoder_ = std::make_unique<VideoDecoder>();
    audio_renderer_ = std::make_unique<SDLAudioRenderer>();
    video_renderer_ = std::make_unique<SDLVideoRenderer>();
    coordinator_ = std::make_unique<PlaybackCoordinator>();
    printf("MediaPlayer created\n");

    }

MediaPlayer::~MediaPlayer(){
    close();
    printf("player close");
}

bool MediaPlayer::openUrl(const std::string& url){
    current_media_url_ = url;
    if(!demuxer_->open(url.c_str())){
        fprintf(stderr,"failed to open url: %s\n", url.c_str());
        return false;
    }

    if(!initializeComponents()){
        fprintf(stderr,"fail to initialize compoments");
        close();
        return false;
    }

    printf("success to open url: %s\n",url.c_str());
    return true;
}
bool MediaPlayer::openFile(const std::string& file_path) {
    return openUrl(file_path);
}

void MediaPlayer::close(){
    stop();
    cleanupComponents();
    if(demuxer_){
        demuxer_->close();
    }
    current_media_url_.clear();
    printf("mediaplayer close");
}

bool MediaPlayer::play(){
    std::lock_guard<std::mutex> lock(state_mutex_);

    if(current_state_ == PLAYING){
        return true;
    }
    
    if(current_state_ == STOPPED){
        if(!coordinator_->start()){
            fprintf(stderr,"Failed to start playback coordinator");
            return false;
        }
    }else if(current_state_ == PAUSED){
        coordinator_->resume();
    }
    current_state_ = PLAYING;
    printf("playback started\n");
    return true;
}

void MediaPlayer::run(){
    if(current_media_url_.empty()){
        fprintf(stderr,"No media file opened\n");
        return;
    }
    auto last_status_time = std::chrono::steady_clock::now();

    while(current_state_ == PLAYING){
        if(hasVideo()){
            renderVideoFrame();
        }
    }
}

void MediaPlayer::pause(){
    std::lock_guard<std::mutex> lock(state_mutex_);
    if(current_state_ != PLAYING){
        return;
    }

    coordinator_->pause();
    current_state_ = PAUSED;
    printf("playback paused\n");
}

void MediaPlayer::resume(){
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (current_state_ != PAUSED) {
        return;
    }
    
    coordinator_->resume();
    current_state_ = PLAYING;
    printf("Playback resumed\n");
}

void MediaPlayer::stop(){
    std::lock_guard<std::mutex> lock(state_mutex_);
    if(current_state_ == STOPPED){
        return;
    }
    current_state_ = STOPPED;
    coordinator_->stop();
    printf("playback stopped");
}

bool MediaPlayer::seek(double time_seconds){
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    PlayState old_state = current_state_;

    bool success = coordinator_->seek(time_seconds);
    return success;
}

bool MediaPlayer::hasAudio() const{
    return demuxer_ ? demuxer_->hasAudio() :false;
}

bool MediaPlayer::hasVideo() const{
    return demuxer_ ? demuxer_->hasVideo() :false;
}

void MediaPlayer::setVolume(float volume){
    printf("set volume");
}


bool MediaPlayer::initializeComponents(){
    try{
        if(demuxer_->hasAudio()){
            if(!audio_decoder_->initializeByStream(demuxer_->get_audiostream())){
                throw std::runtime_error("Failed to initialize audio decoder\n");
            }

            int sample_rate = audio_decoder_->getSampleRate();
            int channels = audio_decoder_->getChannels();

            AVSampleFormat format = audio_decoder_->getSampleFormat();

            if(!audio_renderer_->initialize(sample_rate,channels,format)){
                throw std::runtime_error("Failed to initialize audio render\n");
            }
            printf("Audio pipeline initialized :%d Hz, %d ch\n",sample_rate,channels);
        }

        if(demuxer_->hasVideo()){
            if(!video_decoder_->initializeByStream(demuxer_->get_videostream())){
                throw std::runtime_error("Failed to initialize video decoder\n");
            }
            int width = video_decoder_->getWidth();
            int height = video_decoder_->getHeight();
            AVPixelFormat format = video_decoder_->getPixFmt();

            if(!video_renderer_->initialize(width,height,format)){
                throw std::runtime_error("Failed to initialize video renderer");
            }
            printf("video pipeline initialized: %dx%d\n", width,height);
        }

    coordinator_->setDemuxer(demuxer_.get());
    coordinator_->setAudioDecoder(audio_decoder_.get());
    coordinator_->setVideoDecoder(video_decoder_.get());
    coordinator_->setAudioRenderer(audio_renderer_.get());
    coordinator_->setVideoRenderer(video_renderer_.get());
    
    printf("components initialized successfully\n");
    return true;
    }catch(const std::exception& e){
        fprintf(stderr,"component initialized failed %s",e.what());
        return false;
    }
}

void MediaPlayer::renderVideoFrame() {
    if (!coordinator_ || !video_renderer_) {
        return;
    }
    
    AVFrame* frame = coordinator_->getNextVideoFrame();
    
    if (!frame) {
        int delay_ms = coordinator_->getVideoFrameDelay();
        if (delay_ms > 0 && delay_ms < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return;
    }
    video_renderer_->displayFrame(frame);
    av_frame_free(&frame);
}


void MediaPlayer::cleanupComponents(){
    if(audio_renderer_){
        audio_renderer_->close();
    }

    if(video_renderer_){
        video_renderer_->close();
    }

    printf("components cleaned up\n");
}

Demuxer::MediaInfo MediaPlayer::getMediaInfo(){
    if(demuxer_){
        return demuxer_->getMediaInfo();
    }
    return Demuxer::MediaInfo();
}