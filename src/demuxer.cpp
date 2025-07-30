#include "demuxer.hpp"
#include <iostream>

Demuxer::Demuxer() {
}

Demuxer::~Demuxer() {
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }
}

bool Demuxer::loadfile(const char* src_filename){
    if(avformat_open_input(&fmt_ctx,src_filename,NULL,NULL) < 0){
        fprintf(stderr,"could not open source file %s\n",src_filename);
        exit(1);
    }

    if(avformat_find_stream_info(fmt_ctx,NULL) < 0){
        fprintf(stderr,"Could not find stream information\n");
        return false;
    }
    return true;
}


bool Demuxer::open_video_format(){
    int ret = stream_init(AVMEDIA_TYPE_VIDEO);
    std::cout << "ret: " << ret << std::endl;
    if(ret>=0)
    {
        video_stream_idx = ret;
        video_stream = fmt_ctx->streams[video_stream_idx];
        return true;
    }
    return false;
}


bool Demuxer::open_audio_format(){
    int ret = stream_init(AVMEDIA_TYPE_AUDIO);
    std::cout << "ret: " << ret << std::endl;
    if(ret>=0) 
    {
        audio_stream_idx = ret;
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        return true;
    }
    return false;
}

int Demuxer::stream_init(enum AVMediaType type)
{
    int ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file\n",
                av_get_media_type_string(type));
        exit(1);
    }
    return ret;
}


AVStream* Demuxer::get_audiostream() const {
    if (!audio_stream) {
        fprintf(stderr, "Warning: audio stream not initialized.\n");
    }
    return audio_stream;
}

AVStream* Demuxer::get_videostream() const {
    if (!video_stream) {
        fprintf(stderr, "Warning: video stream not initialized.\n");
    }
    return video_stream;
}

AVFormatContext* Demuxer::get_fmx() const {
    if(!fmt_ctx){
        fprintf(stderr,"Warning : fmt_ctx no init");
    }
    return fmt_ctx;
}

int Demuxer::getVideoStreamIndex() const{
    if(video_stream_idx<0){
        fprintf(stderr,"Warning: no videostream_dix");
    }
    return video_stream_idx;
}
int Demuxer::getAudioStreamIndex() const{
    if(audio_stream_idx<0){
        fprintf(stderr,"Warning: no videostream_dix");
    }
    return audio_stream_idx;
}