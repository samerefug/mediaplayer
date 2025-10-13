#include "FrameBuffer.hpp"

VideoFrameBuffer::VideoFrameBuffer(int width, int height, AVPixelFormat pix_fmt)
    :width_(width),height_(height),pix_fmt_(pix_fmt),frame_count_(0),max_frame_count_(200),stopped_(false){
        frame_size_ = av_image_get_buffer_size(pix_fmt_,width_,height_,1);
    }

VideoFrameBuffer::~VideoFrameBuffer(){
    clear();
}

bool VideoFrameBuffer::addFrame(const uint8_t* frame_data,size_t data_size){
    printf("video add frame pause use");
    return false;
}
bool VideoFrameBuffer::addFrame(AVFrame* frame) {
    if (!frame || frame->format != pix_fmt_ || 
        frame->width != width_ || frame->height != height_) {
        return false;
    }

    std::unique_lock<std::mutex> lock(buffer_mutex_);
    cond_full_.wait(lock,[this] {return buffer_.size() < max_frame_count_ ; });
    buffer_.push(av_frame_clone(frame));
    frame_count_++;
    cond_empty_.notify_one();
    return true;
}

bool VideoFrameBuffer::getFrame(int frame_index, uint8_t*& frame_data) {
    printf("video get frame pause use");
    return false;
}

AVFrame* VideoFrameBuffer::getAVFrame() {
    std::unique_lock<std::mutex>lock(buffer_mutex_);
    cond_empty_.wait(lock,[this]{return stopped_ ||!buffer_.empty();});

    if(stopped_ && buffer_.empty()){
        return nullptr;
    }
    AVFrame* frame = buffer_.front();
    buffer_.pop();
    cond_full_.notify_all();
    return frame;
}

void VideoFrameBuffer::clear() {
    stopped_ = true;
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    //
    while(!buffer_.empty()){
        AVFrame* f = buffer_.front();
        buffer_.pop();
        av_frame_free(&f);
    }
    frame_count_ = 0;
    cond_empty_.notify_all();
}

AudioFrameBuffer::AudioFrameBuffer(int sample_rate, int channels, AVSampleFormat sample_fmt)
    : sample_rate_(sample_rate), channels_(channels), sample_fmt_(sample_fmt), total_samples_(0),max_frame_count_(200),stopped_(false) {
    bytes_per_sample_ = av_get_bytes_per_sample(sample_fmt_);
}

AudioFrameBuffer::~AudioFrameBuffer() {
    clear();
}
bool AudioFrameBuffer::addFrame(const uint8_t* sample_data,int nb_samples){
    printf("audio add frame pause use");
    return false;
}
bool AudioFrameBuffer::addFrame(AVFrame* frame){
    if (!frame || frame->sample_rate != sample_rate_ ||
        frame->format != sample_fmt_ || frame->ch_layout.nb_channels != channels_) {
        return false;
    }

    std::unique_lock<std::mutex> lock(buffer_mutex_);
    cond_full_.wait(lock, [this] { return buffer_.size() < max_frame_count_; });

    buffer_.push(av_frame_clone(frame));
    total_samples_ += frame->nb_samples;
    cond_empty_.notify_one();
    return true;
}

bool AudioFrameBuffer::getSamples(int start_sample, int nb_samples, uint8_t*& sample_data) {
    printf("audio get frame pause use");
    return false;
}

AVFrame* AudioFrameBuffer::getAVFrame() {
    std::unique_lock<std::mutex> lock(buffer_mutex_);
    cond_empty_.wait(lock,[this]{return stopped_ ||!buffer_.empty();});
    if(stopped_ && buffer_.empty()){
        return nullptr;
    }

    AVFrame* frame = buffer_.front();
    buffer_.pop();
    total_samples_ -= frame->nb_samples;
    cond_full_.notify_one();
    return frame;
}

void AudioFrameBuffer::clear() {
    stopped_ = true;
    std::lock_guard<std::mutex> lock(buffer_mutex_);
        while (!buffer_.empty()) {
            AVFrame* f = buffer_.front();
            buffer_.pop();
            av_frame_free(&f);
        }
    total_samples_ = 0;
    cond_empty_.notify_all();
}

bool MediaDataManager::initVideoBuffer(int width, int height, AVPixelFormat pix_fmt) {
    video_buffer_ = std::make_unique<VideoFrameBuffer>(width, height, pix_fmt);
    return video_buffer_ != nullptr;
}

bool MediaDataManager::initAudioBuffer(int sample_rate, int channels, AVSampleFormat sample_fmt) {
    audio_buffer_ = std::make_unique<AudioFrameBuffer>(sample_rate, channels, sample_fmt);
    return audio_buffer_ != nullptr;
}

void MediaDataManager::clearAll() {
    if (video_buffer_) {
        video_buffer_->clear();
    }
    if (audio_buffer_) {
        audio_buffer_->clear();
    }
}