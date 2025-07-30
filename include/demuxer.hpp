#ifndef DEMUXER
#define DEMUXER
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
        
        AVFormatContext* get_fmx() const;
        AVStream* get_audiostream() const;
        AVStream* get_videostream() const;
        int stream_init(enum AVMediaType type) ;
        int getVideoStreamIndex() const;
        int getAudioStreamIndex() const;
        
    private:    
        AVFormatContext *fmt_ctx = NULL;
        AVStream *video_stream;
        AVStream *audio_stream;
        int video_stream_idx;
        int audio_stream_idx;
};

#endif