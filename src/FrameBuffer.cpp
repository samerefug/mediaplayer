#include "FrameBuffer.hpp"

VideoFrameBuffer::VideoFrameBuffer(int width, int height, AVPixelFormat pix_fmt)
    :width_(width),height_(height),pix_fmt_(pix_fmt),frame_count_(0){
        frame_size_ = av_image_get_buffer_size(pix_fmt_,width_,height_,1);
    }

VideoFrameBuffer::~VideoFrameBuffer(){
    clear();
}

bool VideoFrameBuffer::addFrame(const uint8_t* frame_data,size_t data_size){
    if(!frame_data || data_size != frame_size_){
        return false;
    }

    size_t old_size = buffer_.size();
    buffer_.resize(old_size + frame_size_);
    memcpy(buffer_.data()+old_size,frame_data,frame_size_);

    frame_count_++;
    return true;
}

bool VideoFrameBuffer::addFrame(AVFrame* frame) {
    if (!frame || frame->format != pix_fmt_ || 
        frame->width != width_ || frame->height != height_) {
        return false;
    }
    
    size_t old_size = buffer_.size();
    buffer_.resize(old_size + frame_size_);
    
    uint8_t* dst_data[4];
    int dst_linesize[4];
    
    av_image_fill_arrays(dst_data, dst_linesize,
                        buffer_.data() + old_size,
                        pix_fmt_, width_, height_, 1);
    
    av_image_copy(dst_data, dst_linesize,
                  (const uint8_t**)frame->data, frame->linesize,
                  pix_fmt_, width_, height_);
    
    frame_count_++;
    return true;
}

bool VideoFrameBuffer::getFrame(int frame_index, uint8_t*& frame_data) {
    if (frame_index < 0 || frame_index >= frame_count_) {
        return false;
    }
    frame_data = buffer_.data() + frame_index * frame_size_;
    return true;
}

AVFrame* VideoFrameBuffer::getAVFrame(int frame_index) {
    uint8_t* frame_data;
    if (!getFrame(frame_index, frame_data)) {
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return nullptr;
    }

    frame->format = pix_fmt_;
    frame->width = width_;
    frame->height = height_;

    av_image_fill_arrays(frame->data, frame->linesize,
                        frame_data, pix_fmt_, width_, height_, 1);

    return frame;
}

void VideoFrameBuffer::clear() {
    buffer_.clear();
    frame_count_ = 0;
}

AudioFrameBuffer::AudioFrameBuffer(int sample_rate, int channels, AVSampleFormat sample_fmt)
    : sample_rate_(sample_rate), channels_(channels), sample_fmt_(sample_fmt), total_samples_(0) {
    bytes_per_sample_ = av_get_bytes_per_sample(sample_fmt_);
}

AudioFrameBuffer::~AudioFrameBuffer() {
    clear();
}

bool AudioFrameBuffer::addFrame(const uint8_t* samples_data,int nb_samples){
    printf("pause use");
    return true;
}

bool AudioFrameBuffer::addFrame(AVFrame* frame){
    if (!frame || frame->sample_rate != sample_rate_ ||
        frame->format != sample_fmt_ || frame->ch_layout.nb_channels != channels_) {
        return false;
    }

    size_t bytes_per_sample = av_get_bytes_per_sample((AVSampleFormat)sample_fmt_);
    bool planar = av_sample_fmt_is_planar((AVSampleFormat)sample_fmt_);

    for (int i = 0; i < frame->nb_samples; ++i) {
        for (int ch = 0; ch < channels_; ++ch) {
            const uint8_t* src;
            if (planar) {
                src = frame->data[ch] + i * bytes_per_sample;
            } else {
                src = frame->data[0] + (i * channels_ + ch) * bytes_per_sample;
            }
            buffer_.insert(buffer_.end(), src, src + bytes_per_sample);
        }
    }

    total_samples_ += frame->nb_samples;
    return true;
}

bool AudioFrameBuffer::getSamples(int start_sample, int nb_samples, uint8_t*& sample_data) {
    printf("pause use");
    return true;
}

AVFrame* AudioFrameBuffer::getAVFrame(int start_sample, int nb_samples) {
   if (start_sample < 0 || start_sample + nb_samples > total_samples_) {
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) return nullptr;

    frame->format = sample_fmt_;
    frame->sample_rate = sample_rate_;
    frame->nb_samples = nb_samples;
    av_channel_layout_default(&frame->ch_layout, channels_);

    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }

    size_t bytes_per_sample = av_get_bytes_per_sample((AVSampleFormat)sample_fmt_);
    bool planar = av_sample_fmt_is_planar((AVSampleFormat)sample_fmt_);

    const uint8_t* src = buffer_.data() + 
                         start_sample * channels_ * bytes_per_sample;

    if (planar) {
        for (int i = 0; i < nb_samples; ++i) {
            for (int ch = 0; ch < channels_; ++ch) {
                const uint8_t* sample_ptr = src + (i * channels_ + ch) * bytes_per_sample;
                memcpy(frame->data[ch] + i * bytes_per_sample,
                       sample_ptr, bytes_per_sample);
            }
        }
    } else {
        size_t data_size = nb_samples * channels_ * bytes_per_sample;
        memcpy(frame->data[0], src, data_size);
    }

    return frame;
}

void AudioFrameBuffer::clear() {
    buffer_.clear();
    total_samples_ = 0;
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