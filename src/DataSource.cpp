#include "DataSource.hpp"

RawFileDataSource::RawFileDataSource(const std::string& file_path, FileType type)
    : file_path_(file_path), file_type_(type)
    , pcm_sample_rate_(44100), pcm_channels_(2), pcm_format_(AV_SAMPLE_FMT_S16)
    , pcm_samples_per_frame_(1024), pcm_bytes_per_sample_(2)
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
    if (data_manager_) {
        if (file_type_ == PCM_FILE) {
            data_manager_->initAudioBuffer(pcm_sample_rate_, pcm_channels_, AV_SAMPLE_FMT_S32);
        } else if (file_type_ == YUV_FILE) {
            data_manager_->initVideoBuffer(yuv_width_, yuv_height_, AV_PIX_FMT_YUV420P);
        }
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
    std::streamsize bytes_read = file_stream_.gcount();

    while(is_active_ && !file_stream_.eof()){
        file_stream_.read(reinterpret_cast<char*>(temp_buffer.data()), frame_size);
        std::streamsize bytes_read = file_stream_.gcount();

        if(bytes_read > 0){
            int samples_read = bytes_read / (pcm_channels_ * pcm_bytes_per_sample_);
            if(pcm_format_ != AV_SAMPLE_FMT_S32){
                std::vector<int32_t> converted_buffer(samples_read * pcm_channels_);
                //格式转换16位
                 if (pcm_format_ == AV_SAMPLE_FMT_S16) {
                    int16_t* src = reinterpret_cast<int16_t*>(temp_buffer.data());
                    for (int i = 0; i < samples_read * pcm_channels_; i++) {
                        converted_buffer[i] = static_cast<int32_t>(src[i]) << 16; 
                    }
                }

                //待添加多种格式....

                audioBuffer->addFrame(reinterpret_cast<uint8_t*>(converted_buffer.data()), samples_read);
            }else{
                audioBuffer->addFrame(temp_buffer.data(),samples_read);
            }
        }
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
            // 如果格式匹配，直接添加
            if (yuv_format_ == AV_PIX_FMT_YUV420P) {
                videoBuffer->addFrame(frame_buffer.data(), yuv_frame_size_);
            } else {
                // 需要格式转换的情况
                AVFrame* temp_frame = av_frame_alloc();
                if (temp_frame) {
                    temp_frame->format = yuv_format_;
                    temp_frame->width = yuv_width_;
                    temp_frame->height = yuv_height_;
                    
                    av_image_fill_arrays(temp_frame->data, temp_frame->linesize,
                                       frame_buffer.data(), yuv_format_,
                                       yuv_width_, yuv_height_, 1);
                    
                    videoBuffer->addFrame(temp_frame);
                    av_frame_free(&temp_frame);
                }
            }
        } else if (bytes_read > 0) {
            break;
        }
    }
}
