#include "Demuxer.hpp"
#include <iostream>

Demuxer::Demuxer()
    : fmt_ctx_(nullptr)
    , video_stream_(nullptr)
    , audio_stream_(nullptr)
    , video_stream_idx_(-1)
    , audio_stream_idx_(-1)
    {

    }

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const char* src_filename){
    if(fmt_ctx_){
        printf("demuxer already opened, should close\n");
    }

    url_ = src_filename;

    fmt_ctx_ = avformat_alloc_context();
    if(!fmt_ctx_){
        fprintf(stderr,"failed to allocate format context\n");
        return false;
    }

    //set options ,should add future
    AVDictionary* options = nullptr;

    int ret = avformat_open_input(&fmt_ctx_,url_.c_str(),nullptr,&options);
    av_dict_free(&options);

    if(ret<0){
        fprintf(stderr,"failed to open input '%s' \n",url_.c_str());
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
        return false;
    }

    if(!findStreamInfo()){
        close();
        return false;
    }

    if(!findBestStreams()){
        printf("warning: no streams found\n");
    }

    printf("demuxer open success\n");
    return true;
}

//only Video On Demand
bool Demuxer::seekTo(int64_t timestamp){
    if(!fmt_ctx_){
        return false;
    }

    std::lock_guard<std::mutex> lock(read_mutex_);
    printf("Seeking to timestamp: %lld (%.2f seconds)\n", 
           (long long)timestamp, timestamp / 1000000.0);

    int ret = av_seek_frame(fmt_ctx_,-1,timestamp,AVSEEK_FLAG_BACKWARD);
    if(ret < 0){
        fprintf(stderr,"seek failed\n");
        return false;
    }

    printf("seek successful\n");
    return true;
}

void Demuxer::close(){
    std::lock_guard<std::mutex> lock(read_mutex_);
    if(fmt_ctx_){
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    audio_stream_ = nullptr;
    video_stream_ = nullptr;
    audio_stream_idx_ = -1;
    video_stream_idx_ = -1;
    printf("demuxer closed\n");
}
bool Demuxer::findBestStreams(){
    video_stream_idx_ = av_find_best_stream(fmt_ctx_,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
    if(video_stream_idx_ >=0){
        video_stream_ = fmt_ctx_->streams[video_stream_idx_];
        printf("selected video stream: %d\n",video_stream_idx_);
    }

    audio_stream_idx_ = av_find_best_stream(fmt_ctx_,AVMEDIA_TYPE_AUDIO,-1,video_stream_idx_,nullptr,0);
    if(audio_stream_idx_ >=0){
        audio_stream_ = fmt_ctx_->streams[audio_stream_idx_];
        printf("selected video stream: %d\n",audio_stream_idx_);
    }

    return (video_stream_ != nullptr || audio_stream_ != nullptr);
}
bool Demuxer::findStreamInfo(){
    int ret = avformat_find_stream_info(fmt_ctx_,nullptr);
    if(ret < 0){
        fprintf(stderr,"failed to find stream info\n");
        return false;
    }

    printf("found %d streams:\n",fmt_ctx_->nb_streams);

    for(unsigned int i = 0; i < fmt_ctx_->nb_streams; i++){
        AVStream* stream = fmt_ctx_->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;
        
        printf("  Stream %d: ", i);
        
        switch (codecpar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                printf("Video - %s, %dx%d, %.2f fps\n",
                       avcodec_get_name(codecpar->codec_id),
                       codecpar->width, codecpar->height,
                       av_q2d(stream->avg_frame_rate));
                break;
                
            case AVMEDIA_TYPE_AUDIO:
                printf("Audio - %s, %d Hz, %d channels\n",
                       avcodec_get_name(codecpar->codec_id),
                       codecpar->sample_rate,
                       codecpar->ch_layout.nb_channels);
                break;
            //sutitle only info  
            case AVMEDIA_TYPE_SUBTITLE:
                printf("Subtitle - %s\n", avcodec_get_name(codecpar->codec_id));
                break;
                
            default:
                printf("Other - %s\n", avcodec_get_name(codecpar->codec_id));
                break;
        }
    }
    
    return true;
}

AVPacket* Demuxer::readPacket(){
    if(!fmt_ctx_){
        return nullptr;
    }
    
    std::lock_guard<std::mutex> lock(read_mutex_);
    AVPacket* pkt = av_packet_alloc();
    if(!pkt){
        fprintf(stderr ," Failed to allocate packet\n");
        return nullptr;
    }

    int ret = av_read_frame(fmt_ctx_,pkt);
    if(ret < 0){
        av_packet_free(&pkt);

        if(ret == AVERROR_EOF){
            printf("end of file reached\n");
        }else{
            fprintf(stderr,"error reading frame\n");
        }

        return nullptr;
    }
    return pkt;
}

AVStream* Demuxer::get_audiostream() const {
    if (!audio_stream_) {
        fprintf(stderr, "Warning: audio stream not initialized.\n");
    }
    return audio_stream_;
}

AVStream* Demuxer::get_videostream() const {
    if (!video_stream_) {
        fprintf(stderr, "Warning: video stream not initialized.\n");
    }
    return video_stream_;
}

AVFormatContext* Demuxer::get_fmx() const {
    if(!fmt_ctx_){
        fprintf(stderr,"Warning : fmt_ctx no init");
    }
    return fmt_ctx_;
}

int Demuxer::getVideoStreamIndex() const{
    if(video_stream_idx_<0)
        printf("Warning: no videostream_dix");
    return video_stream_idx_;
}
int Demuxer::getAudioStreamIndex() const{
    if(audio_stream_idx_<0)
        printf("Warning: no videostream_dix");
    return audio_stream_idx_;
}

bool Demuxer::hasAudio() const{
    if(audio_stream_idx_<0){
        return false;
    }
    return true;

}

bool Demuxer::hasVideo() const{
    if(video_stream_idx_<0){
        return false;
    }
    return true;
}

int64_t Demuxer::getDuration(){
    if(!fmt_ctx_){
        return 0;
    }

    if(fmt_ctx_->duration != AV_NOPTS_VALUE){
        return fmt_ctx_->duration;
    }
    int64_t max_duration = 0;
    for(unsigned int i = 0; i < fmt_ctx_->nb_streams;i++){
        AVStream* stream = fmt_ctx_->streams[i];
        if(stream->duration != AV_NOPTS_VALUE){
            int64_t duration_us = av_rescale_q(stream->duration,stream->time_base,AV_TIME_BASE_Q);
            max_duration = std::max(max_duration,duration_us);
        }
    }
    return max_duration;
}
Demuxer::MediaInfo Demuxer::getMediaInfo(){
    MediaInfo info;
    if(!fmt_ctx_){
        return info;
    }

    info.duration_us = getDuration();
    info.container_format = fmt_ctx_->iformat->name;

    if(fmt_ctx_->pb){
        info.file_size = avio_size(fmt_ctx_->pb);
        if(info.duration_us > 0){
            info.bitrate_kpbs = (info.file_size * 8.0) / (info.duration_us / 1000000.0) / 1000.0;
        }
    }

    if(video_stream_){
        AVCodecParameters* codecpar = video_stream_->codecpar;
        info.width = codecpar->width;
        info.height = codecpar->height;
        info.video_codec_name = avcodec_get_name(codecpar->codec_id);

        if(video_stream_->avg_frame_rate.den !=0){
            info.fps = av_q2d(video_stream_->avg_frame_rate);
        }else if(video_stream_->r_frame_rate.den !=0){
            info.fps = av_q2d(video_stream_->r_frame_rate);
        }
    }

    if(audio_stream_){
        AVCodecParameters* codecpar = audio_stream_->codecpar;
        info.sample_rate = codecpar->sample_rate;
        info.channels = codecpar->ch_layout.nb_channels;
        info.audio_codec_name = avcodec_get_name(codecpar->codec_id);
    }
    return info;
}

