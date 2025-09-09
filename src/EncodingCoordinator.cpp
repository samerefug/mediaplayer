#include "EncodingCoordinator.hpp"

EncodingCoordinator::EncodingCoordinator() 
    : data_manager_(nullptr)
    , audio_encoder_(nullptr)
    , video_encoder_(nullptr)
    , muxer_(nullptr)
    , is_running_(false)
    , should_stop_(false)
    , sync_av_(true)
    , audio_pts_(0)
    , video_pts_(0) {
}

EncodingCoordinator::~EncodingCoordinator() {
    stop();
}

void EncodingCoordinator::setDataManager(MediaDataManager* data_manager) {
    data_manager_ = data_manager;
}

void EncodingCoordinator::setAudioEncoder(AudioEncoder* audio_encoder) {
    audio_encoder_ = audio_encoder;
    audio_time_base_ = audio_encoder_->getCodecContext()->time_base;
}

void EncodingCoordinator::setVideoEncoder(VideoEncoder* video_encoder) {
    video_encoder_ = video_encoder;
    video_time_base_ = video_encoder_->getCodecContext()->time_base;
}

void EncodingCoordinator::setMuxer(AVMuxer* muxer) {
    muxer_ = muxer;
}

bool EncodingCoordinator::start(){
    if(is_running_){
        return true;
    }

    if(!data_manager_ || !muxer_){
        fprintf(stderr,"Required components ");
        return false;
    }

    should_stop_ = false;
    is_running_ =true;

    try{
        if(audio_encoder_ && data_manager_->getAudioBuffer()){
            audio_thread_ = std::make_unique<std::thread>(&EncodingCoordinator::audioEncodingLoop,this);
        }
        if(video_encoder_ && data_manager_->getVideoBuffer()){
            video_thread_ = std::make_unique<std::thread>(&EncodingCoordinator::videoEncodingLoop,this);
        }
        muxing_thread_ = std::make_unique<std::thread>(&EncodingCoordinator::packetMuxingLoop,this);
        printf("Encoding coordinator started\n");
        return true;
    }catch(const std::exception& e){
        fprintf(stderr, "Failed to start encoding coordinator: %s\n", e.what());
        stop();
        return false;
    }
}

void EncodingCoordinator::stop(){
    should_stop_ = true;
    is_running_ =false;

    packet_queue_cv_.notify_all();
    if(audio_thread_ && audio_thread_->joinable()){
        audio_thread_->join();
        audio_thread_.reset();
    }
    if(video_thread_&& video_thread_->joinable()){
        video_thread_->join();
        video_thread_.reset();
    }
    if(muxing_thread_ && muxing_thread_->joinable()){
        muxing_thread_->join();
        muxing_thread_.reset();
    }

    std::lock_guard<std::mutex> lock(packet_queue_mutex_);
    while(!packet_queue_.empty()){
        auto& pair =packet_queue_.front();
        av_packet_free(&pair.first);
        packet_queue_.pop();
    }

    printf("Encoding coordinator stopped\n");
}

void EncodingCoordinator::audioEncodingLoop(){
    AudioFrameBuffer* audio_buffer = data_manager_->getAudioBuffer();
    if(!audio_buffer){
        fprintf(stderr,"Audio buffer not available\n");
        return;
    }
    int64_t audio_pts = 0;         
    int nb_samples = 1152;       
    while(!should_stop_){
        AVFrame* frame = audio_buffer->getAVFrame(audio_pts,nb_samples);
        if(!frame){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        frame->pts = audio_pts;
        audio_pts += frame->nb_samples;

        if(audio_encoder_->encode(frame)){
            while(AVPacket* pkt = audio_encoder_->getEncodedPacket()){
                if(sync_av_){
                    syncTimestamps(pkt,AVMEDIA_TYPE_AUDIO);
                }

                {
                    std::lock_guard<std::mutex> lock(packet_queue_mutex_);
                    packet_queue_.push({pkt,muxer_->getStreamIndex(AVMEDIA_TYPE_AUDIO)});
                    packet_queue_cv_.notify_one();
                }
            }
        }
        av_frame_free(&frame);
    }

    if(audio_encoder_){
        audio_encoder_->flush();
        while(AVPacket* pkt = audio_encoder_->getEncodedPacket()){
            if(sync_av_){
                syncTimestamps(pkt,AVMEDIA_TYPE_AUDIO);
            }

            {
                std::lock_guard<std::mutex> lock(packet_queue_mutex_);
                packet_queue_.push({pkt,muxer_->getStreamIndex(AVMEDIA_TYPE_AUDIO)});
                packet_queue_cv_.notify_one();
            }
        }
    }
}

void EncodingCoordinator::videoEncodingLoop(){
    VideoFrameBuffer* video_buffer = data_manager_->getVideoBuffer();
    if(!video_buffer){
        fprintf(stderr,"video buffer not available\n");
        return;
    }
    int frame_index = 0;
    while(!should_stop_){
        AVFrame* frame = video_buffer->getAVFrame(frame_index);
        if(!frame){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        frame->pts = frame_index;
        frame_index++;
        if(video_encoder_->encode(frame)){
            while(AVPacket* pkt = video_encoder_->getEncodedPacket()){
                if(sync_av_){
                    syncTimestamps(pkt,AVMEDIA_TYPE_VIDEO);
                }

                {
                    std::lock_guard<std::mutex> lock(packet_queue_mutex_);
                    packet_queue_.push({pkt,muxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO)});
                    packet_queue_cv_.notify_one();
                }
            }
        }
    }
    if(video_encoder_){
        video_encoder_->flush();
        while(AVPacket* pkt = video_encoder_->getEncodedPacket()){
            if(sync_av_){
                syncTimestamps(pkt,AVMEDIA_TYPE_VIDEO);
            }

            {
                std::lock_guard<std::mutex> lock(packet_queue_mutex_);
                packet_queue_.push({pkt,muxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO)});
                packet_queue_cv_.notify_one();
            }
        }
    }
}

void EncodingCoordinator::packetMuxingLoop(){
    AVRational srcTimebase;
    while(!should_stop_){
        std::unique_lock<std::mutex>lock(packet_queue_mutex_);
        packet_queue_cv_.wait(lock,[this]{
            return !packet_queue_.empty() || should_stop_;
        });

        if(should_stop_){
            break;
        }
        auto pair =packet_queue_.front();
        packet_queue_.pop();
        lock.unlock();

        AVPacket* pkt = pair.first;
        int st_index = pair.second;
        pkt->stream_index = st_index;
        if(st_index == muxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO)){
            srcTimebase = video_encoder_->getCodecContext()->time_base;
        }else if(st_index == muxer_->getStreamIndex(AVMEDIA_TYPE_AUDIO)) {
             srcTimebase = audio_encoder_->getCodecContext()->time_base;
        }
        if(muxer_->writePacket(pkt,srcTimebase)){
        }else{
            fprintf(stderr,"Failed to write packet to muxer\n");
        }

        av_packet_free(&pkt);
    }
}

int64_t EncodingCoordinator::getAudioTimestamp() {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    return audio_pts_;
}

int64_t EncodingCoordinator::getVideoTimestamp() {
    std::lock_guard<std::mutex> lock(timestamp_mutex_);
    return video_pts_;
}

void EncodingCoordinator::syncTimestamps(AVPacket* pkt,AVMediaType type){
    //...
  return ;
}