#include "demuxer.hpp"


bool Demuxer::loadfile(const char* src_filename){
    if(avformat_open_input(&fmt_ctx,src_filename,NULL,NULL) < 0){
        fprintf(stderr,"could not open source file %s\n",src_filename);
        exit(1);
    }

    if(avformat_find_stream_info(fmt_ctx,NULL) < 0){
        fprintf(stderr,"Could not find stream information\n");
    }
}


bool Demuxer::open_video_format(){
    int ret = 0;
    if(open_codec_context(&video_stream_idx,&video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0){
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dst_file = fopen(video_dst_filename,"wb");
          if (!video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
            exit(1);
        }
        //后续交给文件管理类处理
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data,video_dst_linesize,width,height,pix_fmt,1);
        if(ret < 0){
             fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
             exit(1);
        }
        video_dst_bufsize = ret;
    }
}

bool Demuxer::open_audio_format(){
    int ret = 0;
    if(open_codec_context(&audio_stream_idx,&audio_dec_ctx,fmt_ctx,AVMEDIA_TYPE_AUDIO)>=0){
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dst_file = fopen(audio_dst_filename,"wb");
    }
}

int Demuxer::open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type){

                              }