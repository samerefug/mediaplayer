#ifndef AVMUXER
#define AVMUXER

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <AudioEncoder.hpp>
#include <VideoEncoder.hpp>
extern "C"{
    #include <libavutil/avassert.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/opt.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/timestamp.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    int64_t next_pts;
    int samples_count;
    AVFrame *frame;
    AVFrame *tmp_frame;
    AVPacket *tmp_pkt;
    float t, tincr, tincr2;
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;

    uint8_t* video_src_data;
    int video_frame_count; 
    int32_t* audio_src_data;
    int audio_frame_size;
    void* callback_ctx;

} OutputStream;



class AVMuxer{
    public:
        AVMuxer(const std::string &url_or_file, const std::string &format_name)
            : url(url_or_file), format(format_name), oc(nullptr) {
                initOutputStream(video_st);
                initOutputStream(audio_st);
            };
        void init(AVDictionary* opt_arg = nullptr);
        void setVideoSrcData(uint8_t* data,int num_frames) { video_st.video_src_data = data; video_st.video_frame_count = num_frames; }
        void setAudioSrcData(int32_t* data, int total_bytes) {
            audio_st.audio_src_data = data;
            audio_st.audio_frame_size = total_bytes;
        }
        void mux();
        void close();

    private:
        void addstream(OutputStream *ost,enum AVCodecID codec_id,BaseEncoder* codec,AVDictionary *opt_arg);
        void open_video(OutputStream *ost);
        void open_audio(OutputStream *ost);
        AVFrame* get_video_frame(OutputStream *ost);
        AVFrame* get_audio_frame(OutputStream *ost);

        void initOutputStream(OutputStream &st);

        int write_video_frame(OutputStream *ost);
        int write_audio_frame(OutputStream *ost);

        int write_frame(OutputStream *ost, AVFrame *frame);

        int muxer_packet_callback(OutputStream* ost, AVPacket* pkt);
        void close_stream(OutputStream *ost);
        const AVOutputFormat *fmt;
        AVFormatContext *oc;
        std::string url;
        std::string format;
        int have_video = 0;
        int have_audio = 0;
        int encode_video = 0;
        int encode_audio = 0;

        AudioEncoder audioEncoder;
        VideoEncoder videoEncoder;
        OutputStream video_st;
        OutputStream audio_st;

};



#endif