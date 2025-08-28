#include "Avmuxer.hpp"
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational time_base = fmt_ctx->streams[pkt->stream_index]->time_base;

    double pts_time = (pkt->pts == AV_NOPTS_VALUE) ? -1 : pkt->pts * av_q2d(time_base);
    double dts_time = (pkt->dts == AV_NOPTS_VALUE) ? -1 : pkt->dts * av_q2d(time_base);
    double dur_time = (pkt->duration <= 0) ? -1 : pkt->duration * av_q2d(time_base);

    printf("pts:%" PRId64 " pts_time:%0.6f "
           "dts:%" PRId64 " dts_time:%0.6f "
           "duration:%" PRId64 " duration_time:%0.6f "
           "stream_index:%d\n",
           pkt->pts, pts_time,
           pkt->dts, dts_time,
           pkt->duration, dur_time,
           pkt->stream_index);
}
bool AVMuxer::init(AVDictionary* opt){
    int ret;
    avformat_alloc_output_context2(&oc, NULL, format.c_str(), url.c_str());
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        return false;
    }

    
    fmt = oc->oformat;
    if(!(fmt->flags & AVFMT_NOFILE)){
        ret = avio_open(&oc->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", url.c_str());
            return false;
        }
    }
    return true;
}

int AVMuxer::addVideoStream(AVCodecParameters* codecpar){
    if(!oc || !codecpar){
        return -1;
    }
    AVStream* st = avformat_new_stream(oc,nullptr);
    if(!st){
        fprintf(stderr,"Could not allocate video stream\n");
        return -1;
    }
    st->id = oc->nb_streams -1;
    int ret = avcodec_parameters_copy(st->codecpar,codecpar);
    if(ret < 0){
        fprintf(stderr,"could not copy video codec parameters\n");
        return -1;
    }

    st->time_base = av_inv_q(av_guess_frame_rate(oc, st, nullptr));
    if (st->time_base.num == 0) {
        st->time_base = (AVRational){1, 25}; // 默认
    }
    StreamInfo info;
    info.st = st;
    info.index = st->index;
    stream_.push_back(info);
    return st->index;

}

int AVMuxer::addAudioStream(AVCodecParameters* codecpar){
     if (!oc || !codecpar) {
        return -1;
    }
    
    AVStream* st = avformat_new_stream(oc, nullptr);
    if (!st) {
        fprintf(stderr, "Could not allocate audio stream\n");
        return -1;
    }
    
    st->id = oc->nb_streams - 1;
    
    int ret = avcodec_parameters_copy(st->codecpar, codecpar);
    if (ret < 0) {
        fprintf(stderr, "Could not copy audio codec parameters\n");
        return -1;
    }
    st->time_base = (AVRational){1, codecpar->sample_rate};
    
    StreamInfo info;
    info.st = st;
    info.index = st->index;
    stream_.push_back(info);
    
    return st->index;
}

int AVMuxer::writePacket(AVPacket* packet) {
    if (!oc || !packet) {
        return -1;
    }
    
    if (packet->stream_index < 0 || packet->stream_index >= (int)oc->nb_streams) {
        fprintf(stderr, "Invalid stream index: %d\n", packet->stream_index);
        return -1;
    }
    
    AVStream* st = oc->streams[packet->stream_index];
    
    av_packet_rescale_ts(packet, st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? 
                        (AVRational){1, 25} : (AVRational){1, 48000}, // 这里需要从编码器获取
                        st->time_base);
    
    log_packet(packet);
    
    int ret = av_interleaved_write_frame(oc, packet);
    if (ret < 0) {
        fprintf(stderr, "Error while writing output packet\n");
        return ret;
    }
    
    return 0;
}

int AVMuxer::writeHeader() {
    int ret = avformat_write_header(oc, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when writing output file header\n");
        return ret;
    }
    
    printf("File header written successfully\n");
    return 0;
}



void AVMuxer::finalize() {
    av_write_trailer(oc);
}

void AVMuxer::close() {
    if (oc) {
        if (!(fmt->flags & AVFMT_NOFILE)) {
            avio_closep(&oc->pb);
        }
        
        avformat_free_context(oc);
        oc = nullptr;
        fmt = nullptr;
    }
    
    stream_.clear();
}

void AVMuxer::log_packet(const AVPacket* pkt) {
    if (!oc || pkt->stream_index >= (int)oc->nb_streams) {
        return;
    }
    
    AVRational time_base = oc->streams[pkt->stream_index]->time_base;
    
    double pts_time = (pkt->pts == AV_NOPTS_VALUE) ? -1 : pkt->pts * av_q2d(time_base);
    double dts_time = (pkt->dts == AV_NOPTS_VALUE) ? -1 : pkt->dts * av_q2d(time_base);
    double dur_time = (pkt->duration <= 0) ? -1 : pkt->duration * av_q2d(time_base);

    printf("pts:%" PRId64 " pts_time:%0.6f "
           "dts:%" PRId64 " dts_time:%0.6f "
           "duration:%" PRId64 " duration_time:%0.6f "
           "stream_index:%d\n",
           pkt->pts, pts_time,
           pkt->dts, dts_time,
           pkt->duration, dur_time,
           pkt->stream_index);
}