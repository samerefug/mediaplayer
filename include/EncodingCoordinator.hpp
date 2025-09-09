#ifndef ENCODINGCOORDINATOR
#define ENCODINGCOORDINATOR

#include <thread>
#include <condition_variable>
#include "VideoEncoder.hpp"
#include "AudioEncoder.hpp"
#include "FrameBuffer.hpp"
#include "Avmuxer.hpp"

class EncodingCoordinator{
public:
    EncodingCoordinator();
    ~EncodingCoordinator();

    void setDataManager(MediaDataManager* data_manager);
    void setAudioEncoder(AudioEncoder* audio_encoder);
    void setVideoEncoder(VideoEncoder* video_encoder);
    void setMuxer(AVMuxer* muxer);
    
    bool start();
    void stop();
    bool isRunning() const { return is_running_; }
    void setAudioVideoSync(bool enable) { sync_av_ = enable; }
private:
    void audioEncodingLoop();
    void videoEncodingLoop();
    void packetMuxingLoop();

    int64_t getAudioTimestamp();
    int64_t getVideoTimestamp();

    void syncTimestamps(AVPacket* pkt,AVMediaType type);

    MediaDataManager* data_manager_;
    AudioEncoder* audio_encoder_;
    VideoEncoder* video_encoder_;
    AVMuxer* muxer_;
    
    std::unique_ptr<std::thread> audio_thread_;
    std::unique_ptr<std::thread> video_thread_;
    std::unique_ptr<std::thread> muxing_thread_;
    
    std::atomic<bool> is_running_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> sync_av_;

    std::queue<std::pair<AVPacket*,int>>packet_queue_;
    std::mutex packet_queue_mutex_;
    std::condition_variable packet_queue_cv_;
    AVRational video_time_base_;
    AVRational audio_time_base_;
    int64_t audio_pts_;
    int64_t video_pts_;
    std::mutex timestamp_mutex_;
};

#endif