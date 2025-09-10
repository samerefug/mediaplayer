#include "DataSource.hpp"

RawFileDataSource::RawFileDataSource(const std::string& file_path, FileType type)
    : file_path_(file_path), file_type_(type)
    , pcm_sample_rate_(44100), pcm_channels_(2), pcm_format_(AV_SAMPLE_FMT_S16)
    , pcm_samples_per_frame_(1152), pcm_bytes_per_sample_(2)
    , yuv_width_(1920), yuv_height_(1080), yuv_format_(AV_PIX_FMT_YUV420P)
    , yuv_fps_(25), yuv_frame_size_(0) {

    }


RawFileDataSource::~RawFileDataSource() {
    close();
}


void RawFileDataSource::setPCMParams(int sample_rate,int channels,AVSampleFormat format,int samples_per_frame){
    pcm_sample_rate_ = sample_rate;
    pcm_channels_ = channels;
    pcm_format_ = format;
    pcm_samples_per_frame_ = samples_per_frame;
    pcm_bytes_per_sample_ = av_get_bytes_per_sample(format);
}

void RawFileDataSource::setYUVParams(int width ,int height ,AVPixelFormat format,int fps){
    yuv_width_ = width;
    yuv_height_ = height;
    yuv_format_ = format;
    yuv_fps_ = fps;
    
    yuv_frame_size_ = av_image_get_buffer_size(format, width, height, 1);
}

bool RawFileDataSource::open(){
    file_stream_.open(file_path_,std::ios::binary);
    if(!file_stream_.is_open()){
        fprintf(stderr,"Could not open raw file %s\n" ,file_path_.c_str());
        return false;
    }
    return true;
}


bool RawFileDataSource::start(){
    if (is_active_ || !file_stream_.is_open()) {
        return false;
    }
    is_active_ = true;
    if (file_type_ == PCM_FILE) {
        read_thread_ = std::make_unique<std::thread>(&RawFileDataSource::readPCMLoop, this);
    } else if (file_type_ == YUV_FILE) {
        read_thread_ = std::make_unique<std::thread>(&RawFileDataSource::readYUVLoop, this);
    }
    return true;
}

void RawFileDataSource::stop(){
    is_active_ = false;
    if (read_thread_ && read_thread_->joinable()) {
        read_thread_->join();
        read_thread_.reset();
    }
}

void RawFileDataSource::close() {
    stop();
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

void RawFileDataSource::readPCMLoop(){
    if(!data_manager_ || !data_manager_->getAudioBuffer()){
        return;
    }

    AudioFrameBuffer* audioBuffer = data_manager_->getAudioBuffer();
    size_t frame_size = pcm_bytes_per_sample_* pcm_channels_ *pcm_samples_per_frame_;
    std::vector<uint8_t> temp_buffer(frame_size);
    std::streamsize bytes_read;

    while(is_active_ && !file_stream_.eof()){
        file_stream_.read(reinterpret_cast<char*>(temp_buffer.data()), frame_size);
        bytes_read = file_stream_.gcount();
        int samples_read = bytes_read /(pcm_channels_ * pcm_bytes_per_sample_);
        if(bytes_read >0){
            AVFrame* source_frame = av_frame_alloc();
            source_frame->sample_rate = pcm_sample_rate_;
            source_frame->format = pcm_format_;
            source_frame->nb_samples = samples_read;
            AVChannelLayout ch_layout;
            av_channel_layout_default(&ch_layout, pcm_channels_);
            av_channel_layout_copy(&source_frame->ch_layout, &ch_layout);
            int ret = av_frame_get_buffer(source_frame, 0);
            if (ret < 0) {
                av_frame_free(&source_frame);
                continue;
            }
            fill_frame_from_pcm(source_frame,temp_buffer.data(),samples_read,bytes_read);
            AVFrame* final_frame;
            if (format_converter_ && format_converter_->needAudioConversion()) {
                final_frame = format_converter_->convertAudio(source_frame);
                av_frame_free(&source_frame);
            }else{
                final_frame = source_frame;
            }
            if (final_frame) {
                audioBuffer->addFrame(final_frame);
            }

            av_frame_free(&final_frame);
            av_channel_layout_uninit(&ch_layout);
        }
        double frame_duration_sec = static_cast<double>(samples_read) / pcm_sample_rate_;
        std::this_thread::sleep_for(std::chrono::duration<double>(frame_duration_sec));
    }
}


void RawFileDataSource::readYUVLoop(){
    if (!data_manager_ || !data_manager_->getVideoBuffer()) {
        return;
    }
    VideoFrameBuffer* videoBuffer = data_manager_->getVideoBuffer();
    std::vector<uint8_t> frame_buffer(yuv_frame_size_);

    while (is_active_ && !file_stream_.eof())
    {
        file_stream_.read(reinterpret_cast<char*>(frame_buffer.data()), yuv_frame_size_);
        std::streamsize bytes_read = file_stream_.gcount();
        if (bytes_read == static_cast<std::streamsize>(yuv_frame_size_)) {
                AVFrame* source_frame = av_frame_alloc();
                source_frame->format =yuv_format_;
                source_frame->width = yuv_width_;
                source_frame->height = yuv_height_;
                av_image_fill_arrays(source_frame->data, source_frame->linesize,
                                       frame_buffer.data(), yuv_format_,
                                       yuv_width_, yuv_height_, 1);
                AVFrame* final_frame = av_frame_clone(source_frame);
                if (format_converter_ && format_converter_->needVideoConversion()) {
                    final_frame = format_converter_->convertVideo(source_frame);
                }
                videoBuffer->addFrame(final_frame);
                av_frame_free(&source_frame);
                av_frame_free(&final_frame);
        } else if (bytes_read > 0) {
            break;
        }
        auto frame_interval = std::chrono::milliseconds(1000 / yuv_fps_);
        std::this_thread::sleep_for(frame_interval);
    }
}

void RawFileDataSource::fill_frame_from_pcm(AVFrame* frame,uint8_t* pcm_data,int samples_read,std::streamsize bytes_read) 
{
    if (av_sample_fmt_is_planar(pcm_format_)) {
        for (int ch = 0; ch < pcm_channels_; ch++) {
            uint8_t* dst = frame->data[ch];
            const uint8_t* src = pcm_data + ch * samples_read * pcm_bytes_per_sample_;
            memcpy(dst, src, samples_read * pcm_bytes_per_sample_);
        }
    } else {
        memcpy(frame->data[0], pcm_data, bytes_read);
    }
}