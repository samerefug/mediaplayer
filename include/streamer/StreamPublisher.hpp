#ifndef STREAMPUSHER
#define STREAMPUSHER

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include "Avmuxer.hpp"

class StreamPublisher {
public:
    StreamPublisher();
    ~StreamPublisher();
    
    bool configure(const std::string& rtmp_url);
    void setMuxer(AVMuxer* muxer);
    bool start();
    void stop();
    bool isActive() const { return is_active_; }

    void onPacketSent(size_t packet_size);
    
    // 统计信息
    struct StreamStats {
        uint64_t bytes_sent = 0;
        double bitrate_kbps = 0.0;
        bool connection_alive = false;
    };
    StreamStats getStats() const;

private:
    void publishingLoop();
    bool reconnect();
    bool isConnectionHealthy();
    void updateRealTimeStats();
    
    AVMuxer* muxer_;
    std::string rtmp_url_;
    std::unique_ptr<std::thread> publish_thread_;
    std::atomic<bool> is_active_;
    std::atomic<bool> should_stop_;
    
    mutable std::mutex stats_mutex_;
    StreamStats stats_;

    std::chrono::high_resolution_clock::time_point last_packet_time_;
     uint64_t last_total_bytes_;
    
    int reconnect_attempts_;
    static const int MAX_RECONNECT_ATTEMPTS = 5;
};

#endif