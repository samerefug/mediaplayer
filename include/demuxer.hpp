extern "C"{
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}


class Demuxer{
    public:
        Demuxer();
        ~Demuxer();

        bool loadfile(const char* src_filename);
        bool open_video_format();
        bool open_audio_format();
    private:
        int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);
        AVFormatContext *fmt_ctx = NULL;
        AVStream *video_stream;
        AVStream *audio_stream;
        int video_stream_idx;
        int audio_stream_idx;
        AVCodecContext *video_dec_ctx ;
        AVCodecContext *audio_dec_ctx;
}