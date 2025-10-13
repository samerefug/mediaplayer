#ifndef DEMUXER
#define DEMUXER

#include <mutex>
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

        bool open(const char* src_filename);
        void close();

        bool findStreamInfo();
        bool findBestStreams();
                
        AVPacket* readPacket();
        bool seekTo(int64_t timestamp);

        bool open_video_format();
        bool open_audio_format();


        AVFormatContext* get_fmx() const;
        AVStream* get_audiostream() const;
        AVStream* get_videostream() const;
        int stream_init(enum AVMediaType type) ;
        int getVideoStreamIndex() const;
        int getAudioStreamIndex() const;

        bool hasVideo() const;
        bool hasAudio() const;

        int64_t getDuration();

        struct MediaInfo {
            int64_t duration_us = 0;
            double fps = 0.0;
            int width = 0, height = 0;
            int sample_rate = 0,channels = 0;
            std::string video_codec_name;
            std::string audio_codec_name;
            std::string container_format;
            int64_t file_size = 0;
            double bitrate_kpbs = 0;
        };

        MediaInfo getMediaInfo();
        
    private:
        std::mutex state_mutex_;    
        AVFormatContext *fmt_ctx_;
        AVStream *video_stream_;
        AVStream *audio_stream_;
        int video_stream_idx_;
        int audio_stream_idx_;
        std::string url_;
        mutable std::mutex read_mutex_;
};

#endif