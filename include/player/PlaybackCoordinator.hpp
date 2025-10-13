#ifndef PLAYBACKCOORDINATOR
#define PLAYBACKCOORDINATOR

#include <thread>
#include <condition_variable>
#include <atomic>
#include "Demuxer.hpp"
#include "AudioDecoder.hpp"
#include "VideoDecoder.hpp"
#include "VideoRenderer.hpp"
#include "AudioRenderer.hpp"
class PlaybackCoordinator{
public:
     PlaybackCoordinator();
    ~PlaybackCoordinator();
    
    void setDemuxer(Demuxer* demuxer);
    void setAudioDecoder(AudioDecoder* audio_decoder);
    void setVideoDecoder(VideoDecoder* video_decoder);
    void setAudioRenderer(AudioRenderer* audio_renderer);
    void setVideoRenderer(VideoRenderer* video_renderer);
    
    bool start();
    void stop();
    void pause();
    void resume();
    bool seek(double time_seconds);

    bool isPlaying() const { return is_playing_; }
    bool isPaused() const { return is_paused_; }
    double getCurrentTime();
    double getDuration();
    
    void setPlaybackSpeed(double speed) { playback_speed_ = speed; }

    bool hasVideoFrameReady();
    AVFrame* getNextVideoFrame();
    int getVideoFrameDelay();

private:
    void demuxingLoop();
    void audioDecodingLoop();
    void videoDecodingLoop();

    double getAudioClock();
    double getVideoClock();
    double getMasterClock();

    void updateVideoClock(double pts);

    void clearPacketQueues();
    
    Demuxer* demuxer_;
    AudioDecoder* audio_decoder_;
    VideoDecoder* video_decoder_;

    AudioRenderer* audio_renderer_; 
    VideoRenderer* video_renderer_;

    std::unique_ptr<std::thread> demux_thread_;
    std::unique_ptr<std::thread> audio_decode_thread_;
    std::unique_ptr<std::thread> video_decode_thread_;
    std::unique_ptr<std::thread> audio_render_thread_;
    std::unique_ptr<std::thread> video_render_thread_;
    
    std::queue<AVPacket*> audio_packet_queue_;
    std::queue<AVPacket*> video_packet_queue_;
    std::mutex audio_packet_mutex_;
    std::mutex video_packet_mutex_;
    std::condition_variable audio_packet_cv_;
    std::condition_variable video_packet_cv_;

    std::queue<AVFrame*> decode_audio_queue_;
    std::queue<AVFrame*> decode_video_queue_;
    std::mutex video_frame_mutex_;
    std::mutex audio_frame_mutex_;
    std::condition_variable video_frame_cv_;
    std::condition_variable audio_frame_cv_;

    std::atomic<bool> is_playing_;
    std::atomic<bool> is_paused_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> enable_av_sync_;
    std::atomic<double> playback_speed_;
    
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point last_time_;
    double audio_clock_;
    double video_clock_;
    std::mutex clock_mutex_;
    
    static const size_t MAX_AUDIO_QUEUE_SIZE = 50;
    static const size_t MAX_VIDEO_QUEUE_SIZE = 25;

    static constexpr double SYNC_THRESHOLD = 0.1;      
    static constexpr double MAX_FRAME_DELAY = 0.3;     
    static constexpr double MIN_FRAME_DELAY = 0.001; 
};

#endif