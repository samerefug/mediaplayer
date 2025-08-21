// #include "StreamPusher.hpp"

// bool StreamPusher::init(int width, int height, int fps, int sample_rate, int channels) {
//     // 创建输出上下文
//     avformat_alloc_output_context2(&ofmt_ctx, nullptr, "flv", rtmp_url.c_str());
//     if(!ofmt_ctx) return false;

//     // 初始化视频/音频流
//     if(!initVideoStream(width, height, fps)) return false;
//     if(!initAudioStream(sample_rate, channels)) return false;

//     // 打开输出 IO
//     if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
//         if(avio_open(&ofmt_ctx->pb, rtmp_url.c_str(), AVIO_FLAG_WRITE) < 0) return false;
//     }

//     // 写文件头
//     if(avformat_write_header(ofmt_ctx, nullptr) < 0) return false;

//     return true;
// }

// bool StreamPusher::pushVideoFrame(uint8_t* yuv_data){
//     AVPacket* pkt = encodeVideoFrame(yuv_data);
//     if(!pkt){
//     fprintf(stderr,"encode audio frame fail");
//     exit(1);
//     }
//     pkt->stream_index = video_st->index;
//     av_packet_rescale_ts(pkt,video_codec_ctx->time_base,video_st->time_base);
//     av_interleaved_write_frame(ofmt_ctx,pkt);
//     av_packet_free(&pkt);
//     video_pts++;
//     return true;
// }

// bool StreamPusher::pushAudioFrame(uint8_t* pcm_data , int nb_samples){
//     AVPacket* pkt = encodeAudioFrame(pcm_data,nb_samples);
//     if(!pkt){
//         fprintf(stderr,"encode audio frame fail");
//         exit(1);
//     }
//     pkt->stream_index = audio_st->index;
//     av_packet_rescale_ts(pkt,audio_codec_ctx->time_base,audio_st->time_base);
//     av_interleaved_write_frame(ofmt_ctx,pkt);
//     audio_pts += nb_samples;
//     return true;
// }

// void StreamPusher::close() {
//     if(ofmt_ctx) {
//         av_write_trailer(ofmt_ctx);
//         if(!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
//             avio_close(ofmt_ctx->pb);
//         }
//         avformat_free_context(ofmt_ctx);
//         ofmt_ctx = nullptr;
//     }
// }
