#ifndef LIVESTREAMER
#define LIVESTREAMER

#include <string>
#include <memory>
#include "DataSource.hpp"
#include "FrameBuffer.hpp"
#include "Avmuxer.hpp"
#include "AudioEncoder.hpp"
#include "VideoEncoder.hpp"
#include "EncodingCoordinator.hpp"
#include "StreamPublisher.hpp"
#include "FormatConverter.hpp"

extern "C"{
    #include <libavcodec/avcodec.h>
}


class LiverStreamer{

public:
    LiverStreamer();
    ~LiverStreamer();

    struct Config{
        std::string audio_file;
        int audio_sample_rate = 48000;
        int audio_channels = 2;
        int audio_bitrate = 128000;
        AVCodecID audio_codec = AV_CODEC_ID_MP2;

        std::string video_file;
        int video_width = 1920;
        int video_height = 1080;
        int video_fps = 30;
        int video_bitrate = 200000;
        AVCodecID video_codec = AV_CODEC_ID_H264;

        std::string rtmp_url;
        std::string output_format = "mp4";

        bool enable_av_sync = true;
        bool auto_reconnect = true;
    };

    bool configure(const Config& config);
    bool start();
    void stop();
    bool isStreaming() const;

    void pause();
    void resume();

private:
    bool initializeComponents();
    void cleanupComponents();
    Config config_;

    std::unique_ptr<RawFileDataSource> audio_source_;
    std::unique_ptr<RawFileDataSource> video_source_;
    std::unique_ptr<MediaDataManager> data_manager_;
    std::unique_ptr<AudioEncoder> audio_encoder_;
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<AVMuxer> muxer_;
    std::unique_ptr<EncodingCoordinator> coordinator_;
    std::unique_ptr<StreamPublisher> publisher_;
    std::unique_ptr<MediaFormatConverter> formatConverter_;

    mutable std::mutex status_mutex_;
};

#endif