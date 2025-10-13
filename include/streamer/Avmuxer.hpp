#ifndef AVMUXER
#define AVMUXER

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <streamer/AudioEncoder.hpp>
#include <streamer/VideoEncoder.hpp>
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

class StreamPublisher;
class AVMuxer{
    public:
        AVMuxer(const std::string &url_or_file, const std::string &format_name)
            : url(url_or_file), format(format_name), oc(nullptr) {}
        ~AVMuxer(){close();}
        bool init(AVDictionary* opt_arg = nullptr);
        void setStreamPublisher(StreamPublisher* publisher) {stream_publisher_ = publisher;}
        int writePacket(AVPacket* pkt,AVRational timeBase);
        int addVideoStream(AVCodecParameters* codecpar);
        int addAudioStream(AVCodecParameters* codecpar);
        int writeHeader();
        int getStreamIndex(AVMediaType type) const;
        void finalize();
        void close();
        AVRational getStreamTimeBase(AVMediaType type) const;
    public:
        std::string url;
        std::string format;

    private:
        AVFormatContext* oc;
        const AVOutputFormat* fmt;

        struct StreamInfo{
            AVStream* st;
            int index;
        };

        StreamPublisher* stream_publisher_;
        std::vector<StreamInfo>stream_;
        std::mutex write_mutex_;

        void log_packet(const AVPacket* pkt);
        void notifyPacketWritten(size_t packet_size);
};



#endif