#include "PlaybackCoordinator.hpp"

PlaybackCoordinator::PlaybackCoordinator()
    :demuxer_(nullptr)
    ,audio_decoder_(nullptr)
    ,video_decoder_(nullptr)
    ,audio_renderer_(nullptr)
    ,video_renderer_(nullptr)
    ,is_playing_(false)
    ,is_paused_(false)
    ,should_stop_(false)
    ,enable_av_sync_(true)
    ,playback_speed_(1.0)
    ,audio_clock_(0.0)
    ,video_clock_(0.0) 

{
    start_time_ = std::chrono::high_resolution_clock::now();
}

PlaybackCoordinator::~PlaybackCoordinator(){

}

void PlaybackCoordinator::setDemuxer(Demuxer* demuxer) {
    demuxer_ = demuxer;
}

void PlaybackCoordinator::setAudioDecoder(AudioDecoder* audio_decoder) {
    audio_decoder_ = audio_decoder;
}

void PlaybackCoordinator::setVideoDecoder(VideoDecoder* video_decoder) {
    video_decoder_ = video_decoder;
}

void PlaybackCoordinator::setAudioRenderer(AudioRenderer* audio_renderer) {
    audio_renderer_ = audio_renderer;
}

void PlaybackCoordinator::setVideoRenderer(VideoRenderer* video_renderer) {
    video_renderer_ = video_renderer;
}

bool PlaybackCoordinator::start(){
    if(is_playing_){
        return true;
    }

    if(!demuxer_){
        fprintf(stderr,"Demuxer not set\n");
        return false;
    }

    should_stop_ = false;
    is_playing_ = true;
    is_paused_ = false;

    start_time_ = std::chrono::high_resolution_clock::now();
    {
        std::lock_guard<std::mutex> lock(clock_mutex_);
        audio_clock_ = 0.0;
        video_clock_ = 0.0;
    }

    if (audio_renderer_ && audio_decoder_) {
        audio_renderer_->setTimeBase(audio_decoder_->getTimeBase());
        audio_renderer_->setClockUpdateCallback([this](double pts) {
            audio_clock_ = pts;
        });
    }

    try{
        demux_thread_ = std::make_unique<std::thread>(&PlaybackCoordinator::demuxingLoop,this);

        if(audio_decoder_ && demuxer_->getAudioStreamIndex() >= 0){
            audio_decode_thread_ = std::make_unique<std::thread>(&PlaybackCoordinator::audioDecodingLoop,this);
            //audio_render_thread_ = std::make_unique<std::thread>(&PlaybackCoordinator::audioRenderingLoop,this);
        }

        if(video_decoder_ && demuxer_->getVideoStreamIndex() >=0){
            video_decode_thread_ = std::make_unique<std::thread>(&PlaybackCoordinator::videoDecodingLoop,this);
            //video_render_thread_ = std::make_unique<std::thread>(&PlaybackCoordinator::videoRenderingLoop,this);
        }

        printf("start playing");
        return true;
    }catch(const std::exception& e){
        fprintf(stderr,"Failed to start playback threads: %s\n",e.what());
        stop();
        return false;
    }
}

void PlaybackCoordinator::stop(){
    should_stop_ = true;
    is_playing_ = false;
    is_paused_ = false;

    audio_packet_cv_.notify_all();
    video_packet_cv_.notify_all();

    video_frame_cv_.notify_all();
    audio_frame_cv_.notify_all();

    if(demux_thread_ && demux_thread_->joinable()){
        demux_thread_->join();
        demux_thread_.reset();
    }

    if(audio_decode_thread_ && audio_decode_thread_->joinable()){
        audio_decode_thread_->join();
        audio_decode_thread_.reset();
    }

    if(video_decode_thread_ && video_decode_thread_->joinable()){
        video_decode_thread_->join();
        video_decode_thread_.reset();
    }

    if(audio_render_thread_ && audio_render_thread_->joinable()){
        audio_render_thread_->join();
        audio_render_thread_.reset();
    }

    if(video_render_thread_ && video_render_thread_->joinable()){
        video_render_thread_->join();
        video_render_thread_.reset();
    }
    clearPacketQueues();
    if(audio_renderer_){
        audio_renderer_->pause();
    }
    printf("stop playing");
}

void PlaybackCoordinator::pause(){
    if(!is_playing_ || is_paused_){
        return;
    }

    is_paused_ = true;

    if(audio_renderer_){
        audio_renderer_->pause();
    }

    printf("playback paused\n");
}

void PlaybackCoordinator::resume(){
    if(!is_playing_ || !is_paused_){
        return;
    }

    is_paused_ = true;
    start_time_ = std::chrono::high_resolution_clock::now();
    
    if (audio_renderer_) {
        audio_renderer_->resume();
    }
    
    printf("Playback resumed\n");
}


double PlaybackCoordinator::getCurrentTime() {
    // if (enable_av_sync_) {
    //     return getMasterClock();
    // } else {

    // }
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    return elapsed.count() / 1000000.0 * playback_speed_;
}

double PlaybackCoordinator::getAudioClock() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    return audio_clock_;
}

double PlaybackCoordinator::getVideoClock() {
    std::lock_guard<std::mutex> lock(clock_mutex_);
    return video_clock_;
}

double PlaybackCoordinator::getMasterClock() {
    if (demuxer_->hasAudio()) {
        return getAudioClock();
    } else if (demuxer_->hasVideo()) {
        return getVideoClock();
    } else {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
        return elapsed.count() / 1000000.0 * playback_speed_;
    }
}

double PlaybackCoordinator::getDuration() {

}

bool PlaybackCoordinator::seek(double time_seconds){
    if(demuxer_){
        return false;
    }

    printf("seeking to %.2f seconds\n",time_seconds);

    bool was_playing = is_playing_ && !is_paused_;
    pause();

    clearPacketQueues();
    int64_t timestamp = time_seconds * AV_TIME_BASE;
    if(!demuxer_->seekTo(timestamp)){
        fprintf(stderr,"seek failed\n");
        return false;
    }

    if(audio_decoder_){
        audio_decoder_->flush();
    }

    if(video_decoder_){
        video_decoder_->flush();
    }

    return true;
}
void PlaybackCoordinator::demuxingLoop(){
    printf("demuxing thread started\n");
    while(!should_stop_){
        if(is_paused_){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

      AVPacket* pkt = demuxer_->readPacket();
        if(!pkt){
            printf("end of stream reached\n");
            break;
        }

        if(pkt->stream_index == demuxer_->getAudioStreamIndex()){
            std::unique_lock<std::mutex> lock(audio_packet_mutex_);
            audio_packet_cv_.wait(lock, [&]{ return audio_packet_queue_.size() < MAX_AUDIO_QUEUE_SIZE|| should_stop_; });
            audio_packet_queue_.push(pkt);
            audio_packet_cv_.notify_one();
        }else if(pkt->stream_index == demuxer_->getVideoStreamIndex()){
            std::unique_lock<std::mutex> lock(video_packet_mutex_);
            video_packet_cv_.wait(lock, [&]{ return video_packet_queue_.size() < MAX_VIDEO_QUEUE_SIZE|| should_stop_; });
            video_packet_queue_.push(pkt);
            video_packet_cv_.notify_one();
        }else{
            av_packet_free(&pkt);
        }
    }
}

void PlaybackCoordinator::audioDecodingLoop(){
printf("Audio decoding thread started\n");
    
    while (!should_stop_) {
        AVPacket* pkt = nullptr;
        {
            std::unique_lock<std::mutex> lock(audio_packet_mutex_);
            audio_packet_cv_.wait(lock, [this] {
                return !audio_packet_queue_.empty() || should_stop_;
            });
            
            if (should_stop_) break;
            
            pkt = audio_packet_queue_.front();
            audio_packet_queue_.pop();
        }
        audio_packet_cv_.notify_one();
        if (audio_decoder_->decodePacket(pkt)) {
            while (AVFrame* frame = audio_decoder_->getDecodedFrame()) {
                if (should_stop_) {
                    av_frame_free(&frame);
                    break;
                }
                
                if (!audio_renderer_->playFrame(frame)) {
                    fprintf(stderr, "Audio rendering failed\n");
                    av_frame_free(&frame);
                    break;
                }

                av_frame_free(&frame);
            }
        }
        
        av_packet_free(&pkt);
    }
    
    printf("Audio decoding thread ended\n");
}


void PlaybackCoordinator::updateVideoClock(double pts) {
    video_clock_ = pts;
}


void PlaybackCoordinator::videoDecodingLoop(){
    printf("video decoding thread started\n");
    while(!should_stop_){
        AVPacket* pkt = nullptr;
        {
            std::unique_lock<std::mutex> lock(video_packet_mutex_);
            video_packet_cv_.wait(lock,[this]{return !video_packet_queue_.empty() || should_stop_;});

            if(should_stop_) break;

            pkt = video_packet_queue_.front();
            video_packet_queue_.pop();
            video_packet_cv_.notify_one();
        }
        if(video_decoder_->decodePacket(pkt)){
            while(AVFrame* frame = video_decoder_->getDecodedFrame()){
                if(should_stop_){
                    av_frame_free(&frame);
                    break;
                }
                {
                    std::lock_guard<std::mutex> lock(video_frame_mutex_);
                    decode_video_queue_.push(frame);
                }
                video_frame_cv_.notify_one();
            }
        }
        av_packet_free(&pkt);
    }
    printf("video decoding thread ended\n");
}

void PlaybackCoordinator::clearPacketQueues() {
    {
        std::lock_guard<std::mutex> lock(audio_packet_mutex_);
        while (!audio_packet_queue_.empty()) {
            AVPacket* packet = audio_packet_queue_.front();
            audio_packet_queue_.pop();
            av_packet_free(&packet);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(video_packet_mutex_);
        while (!video_packet_queue_.empty()) {
            AVPacket* packet = video_packet_queue_.front();
            video_packet_queue_.pop();
            av_packet_free(&packet);
        }
    }
}

bool PlaybackCoordinator::hasVideoFrameReady() {
    std::lock_guard<std::mutex> lock(video_frame_mutex_);
    return !decode_video_queue_.empty();
}

//according to audio
AVFrame* PlaybackCoordinator::getNextVideoFrame() {
    std::lock_guard<std::mutex> lock(video_frame_mutex_);
    
    while (!decode_video_queue_.empty()) {
        AVFrame* frame = decode_video_queue_.front();
        
        double frame_pts = 0.0;
        if (frame->pts != AV_NOPTS_VALUE) {
            AVRational time_base = demuxer_->get_videostream()->time_base;
            frame_pts = frame->pts * av_q2d(time_base);
        }
        if (enable_av_sync_ && demuxer_->hasAudio()) {
            double audio_time = getAudioClock();
            double diff = frame_pts - audio_time;
            
            if (diff > SYNC_THRESHOLD) {
                return nullptr;
            } else if (diff < -SYNC_THRESHOLD * 30) {
                printf("Dropping video frame, lagging by %.3f seconds\n", -diff);
                decode_video_queue_.pop();
                av_frame_free(&frame);
                video_frame_cv_.notify_one();

                continue;
            }
        }

        decode_video_queue_.pop();
        updateVideoClock(frame_pts);
        video_frame_cv_.notify_one();
        
        return frame;
    }
    
    return nullptr;
}

//25fps rendering
// AVFrame* PlaybackCoordinator::getNextVideoFrame() {
//     std::unique_lock<std::mutex> lock(video_frame_mutex_);

//     if (decode_video_queue_.empty() || should_stop_) {
//         return nullptr;
//     }
//     auto now = std::chrono::high_resolution_clock::now();
//     double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time_).count() / 1e6;

//     const double frame_interval = 1.0 / 25.0;
//     if (elapsed < frame_interval) {
//         return nullptr;
//     }

//     AVFrame* frame = decode_video_queue_.front();
//     decode_video_queue_.pop();
//     video_frame_cv_.notify_one();

//     last_time_ = now;

//     updateVideoClock(frame_interval);

//     return frame;
// }

int PlaybackCoordinator::getVideoFrameDelay() {
    std::lock_guard<std::mutex> lock(video_frame_mutex_);
    
    if (decode_video_queue_.empty()) {
        return 10;
    }
    
    AVFrame* frame = decode_video_queue_.front();
    
    if (frame->pts == AV_NOPTS_VALUE) {
        return 0;
    }
    
    AVRational time_base = demuxer_->get_videostream()->time_base;
    double frame_pts = frame->pts * av_q2d(time_base);
    
    if (enable_av_sync_ && demuxer_->hasAudio()) {
        double audio_time = getAudioClock();
        double diff = frame_pts - audio_time;
        
        if (diff > SYNC_THRESHOLD) {
            int delay_ms = static_cast<int>(diff * 1000);
            return std::min(delay_ms, 100);
        }
    }
    
    return 0;
}