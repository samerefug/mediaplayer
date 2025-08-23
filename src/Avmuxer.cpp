#include "Avmuxer.hpp"
#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
 
#define SCALE_FLAGS SWS_BICUBIC

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



void AVMuxer::open_video(OutputStream *ost){
    int ret;
    AVCodecContext *c = ost->enc;
    /* allocate and init a reusable frame */
    ost->frame = alloc_video_frame(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
 
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_video_frame(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary video frame\n");
            exit(1);
        }
    }
 
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

void AVMuxer::open_audio(OutputStream *ost){
    int nb_samples,ret;
    AVCodecContext* c = ost->enc;
    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;
    ost->frame = alloc_audio_frame(c->sample_fmt, &c->ch_layout,c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S32,  &c->ch_layout,c->sample_rate, nb_samples);
      /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
 
    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
    }
 
    /* set options */
    av_opt_set_chlayout  (ost->swr_ctx, "in_chlayout",       &c->ch_layout,      0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S32, 0);
    av_opt_set_chlayout  (ost->swr_ctx, "out_chlayout",      &c->ch_layout,      0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
 
    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        exit(1);
    }
}

void AVMuxer::initOutputStream(OutputStream &st) {
    memset(&st, 0, sizeof(OutputStream));
    st.st = nullptr;
    st.enc = nullptr;
    st.next_pts = 0;
    st.samples_count = 0;
    st.frame = nullptr;
    st.tmp_frame = nullptr;
    st.tmp_pkt = nullptr;
    st.t = 0;
    st.tincr = 0;
    st.tincr2 = 0;
    st.sws_ctx = nullptr;
    st.swr_ctx = nullptr;
    st.video_src_data = nullptr;
    st.audio_src_data = nullptr;
    st.audio_frame_size = 0;
    st.callback_ctx = nullptr;
}


void fill_frame_from_memory(AVFrame* frame, uint8_t* src_data, int width, int height) {
    int y_size = width * height;
    int uv_size = (width/2) * (height/2);

    for (int i = 0; i < height; i++) {
        memcpy(frame->data[0] + i * frame->linesize[0],
               src_data + i * width,
               width);
    }

    for (int i = 0; i < height/2; i++) {
        memcpy(frame->data[1] + i * frame->linesize[1],
               src_data + y_size + i * (width/2),
               width/2);
    }

    for (int i = 0; i < height/2; i++) {
        memcpy(frame->data[2] + i * frame->linesize[2],
               src_data + y_size + uv_size + i * (width/2),
               width/2);
    }
}



AVFrame* AVMuxer:: get_video_frame(OutputStream *ost)
{
       AVCodecContext *c = ost->enc;
    int frame_size = c->width * c->height * 3 / 2; // YUV420P
    int total_frames = ost->video_frame_count;      // 记录视频缓冲总帧数
    uint8_t* frame_ptr = ost->video_src_data + ost->next_pts * frame_size;

    // 结束条件：读取完缓冲区所有帧
    if (ost->next_pts >= total_frames)
        return nullptr;

    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);

    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          AV_PIX_FMT_YUV420P,
                                          c->width, c->height,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr, "Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_frame_from_memory(ost->tmp_frame, frame_ptr, c->width, c->height);
        sws_scale(ost->sws_ctx, (const uint8_t * const *) ost->tmp_frame->data,
                  ost->tmp_frame->linesize, 0, c->height, ost->frame->data,
                  ost->frame->linesize);
    } else {
        fill_frame_from_memory(ost->frame, frame_ptr, c->width, c->height);
    }

    ost->frame->pts = ost->next_pts++;
    return ost->frame;
}

AVFrame* AVMuxer::get_audio_frame(OutputStream *ost){

    AVCodecContext *c = ost->enc;
    AVFrame *frame = ost->tmp_frame;
    int channels = c->channels;
    int bytes_per_sample = av_get_bytes_per_sample(c->sample_fmt);

    // 计算总样本数（每个声道）
    int total_samples = ost->audio_frame_size / (bytes_per_sample * channels);

    // 检查是否还有数据可生成
    if (ost->next_pts >= total_samples)
        return nullptr;

    // 本帧需要的样本数
    int nb_samples = frame->nb_samples;
    if (ost->next_pts + nb_samples > total_samples)
        nb_samples = total_samples - ost->next_pts; // 最后一帧可能不足

    // 确保 AVFrame 可写
    if (av_frame_make_writable(frame) < 0)
        exit(1);

    // 复制 PCM 数据到 AVFrame
    int32_t* dst = (int32_t*)frame->data[0];
    const int32_t* src = ost->audio_src_data + ost->next_pts * channels;
    for (int i = 0; i < nb_samples * channels; i++)
        dst[i] = src[i];

    // 设置帧样本数和时间戳
    frame->nb_samples = nb_samples;
    frame->pts = ost->next_pts;
    ost->next_pts += nb_samples;

    return frame;
}
 

int AVMuxer::write_audio_frame(OutputStream *ost)
{
    AVCodecContext *c;
    AVFrame *frame;
    int ret;
    int64_t dst_nb_samples;
 
    c = ost->enc;
 
    frame = get_audio_frame(ost);
 
    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples;
        av_assert0(dst_nb_samples == frame->nb_samples);
 
        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
            exit(1);
 
        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            exit(1);
        }
        frame = ost->frame;
 
        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }
 
    return write_frame(ost, frame);
}


int AVMuxer::write_video_frame(OutputStream *ost)
{
    return write_frame(ost, get_video_frame(ost));
}

int AVMuxer::write_frame(OutputStream *ost, AVFrame *frame)
{
    int ret;

    switch (ost->enc->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            ret = audioEncoder.encode(frame, [ost,this](AVPacket* pkt){return muxer_packet_callback(ost, pkt);});
            break;
        case AVMEDIA_TYPE_VIDEO:
            ret = videoEncoder.encode(frame, [ost,this](AVPacket* pkt){return muxer_packet_callback(ost, pkt);});
            break;
        default:
            fprintf(stderr, "Unknown codec type\n");
            exit(1);
    }
    //ret返回值待添加
    return ret == AVERROR_EOF ? 1 : 0;
}

int AVMuxer::muxer_packet_callback(OutputStream* ost, AVPacket* pkt) {
    AVMuxer* muxer = static_cast<AVMuxer*>(ost->callback_ctx);

    av_packet_rescale_ts(pkt, ost->enc->time_base, ost->st->time_base);
    pkt->stream_index = ost->st->index;

    log_packet(muxer->oc, pkt);
    int ret = av_interleaved_write_frame(muxer->oc, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error while writing output packet\n");
    }
    av_packet_ref(ost->tmp_pkt, pkt);
}

void AVMuxer::addstream(OutputStream *ost,enum AVCodecID codec_id,BaseEncoder* encoder,AVDictionary *opt_arg){
    
    int i;    
    ost->tmp_pkt = av_packet_alloc();
    if(!ost->tmp_pkt){
        fprintf(stderr ,"Could not allocate AVPAcket\n");
        exit(1);
    }

    ost->st = avformat_new_stream(oc,NULL);
    if(!ost->st){
        fprintf(stderr,"Could not allocate stream\n");
        exit(1);
    }

    ost->st->id = oc->nb_streams-1;
    encoder->init(nullptr, AV_CODEC_ID_NONE, opt_arg);
    ost->enc = encoder->getCodecContext();

    switch (ost->enc->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            ost->st->time_base = (AVRational){1, ost->enc->sample_rate};
            ost->callback_ctx = this; 
            break;
        case AVMEDIA_TYPE_VIDEO:
            ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
            ost->callback_ctx = this; 
            break;
        default:
            fprintf(stderr, "Unknown codec type\n");
            exit(1);
    }
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        ost->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

void AVMuxer::init(AVDictionary* opt){
    int ret;
    avformat_alloc_output_context2(&oc, NULL, format.c_str(), url.c_str());
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", url.c_str());
    }
    if (!oc)
        return;
    
    fmt = oc->oformat;
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        addstream(&video_st, fmt->video_codec,&videoEncoder,opt);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        addstream(&audio_st, fmt->audio_codec,&audioEncoder,opt);
        have_audio = 1;
        encode_audio = 1;
    }

    if (have_video)
        open_video(&video_st);
 
    if (have_audio)
        open_audio(&audio_st);

    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s':\n", url.c_str());
            return ;
        }
    }

    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: \n");
        return;
    }
}

void AVMuxer::mux(){
     while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                                            audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(&video_st);
        } else {
            encode_audio = !write_audio_frame(&audio_st);
        }
    }

    av_write_trailer(oc);
}


void  AVMuxer::close_stream(OutputStream *ost)
{
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    av_packet_free(&ost->tmp_pkt);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}
 

void AVMuxer::close(){
    /* Close each codec. */
    if (have_video)
        close_stream(&video_st);
    if (have_audio)
        close_stream(&audio_st);
 
    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);
 
    /* free the stream */
    avformat_free_context(oc);
}