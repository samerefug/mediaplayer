#include "StreamPublisher.hpp"

StreamPublisher::StreamPublisher()
     : muxer_(nullptr)
    , is_active_(false)
    , should_stop_(false)
    , last_total_bytes_(0)
    , reconnect_attempts_(0) 
{

}

StreamPublisher::~StreamPublisher() {
    stop();
}

bool StreamPublisher::configure(const std::string& rtmp_url){
     if (rtmp_url.empty()) {
        fprintf(stderr, "RTMP URL is empty\n");
        return false;
    }

    rtmp_url_ = rtmp_url;
    if (rtmp_url_.find("rtmp://") != 0) {
        fprintf(stderr, "Invalid RTMP URL format: %s\n", rtmp_url_.c_str());
        return false;
    }

    printf("Stream publisher configured for: %s\n", rtmp_url_.c_str());
    return true;
}

void StreamPublisher::setMuxer(AVMuxer* muxer){
    muxer_ = muxer;
}

bool StreamPublisher::start(){
    if(is_active_){
        return true;
    }
    if(!muxer_){
        printf("should set muxer");
        return false;
    }

    if(rtmp_url_.empty()){
        printf("should set rtmp_url");
        return false;
    }
    should_stop_ = false;
    is_active_ = true;
    reconnect_attempts_ = 0;
    try {
        publish_thread_ = std::make_unique<std::thread>(&StreamPublisher::publishingLoop, this);
        printf("Stream publisher started\n");
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to start publishing thread: %s\n", e.what());
        is_active_ = false;
        return false;
    }
}

void StreamPublisher::stop(){
    is_active_ = false;
    should_stop_ = true;

    if(publish_thread_ && publish_thread_->joinable()){
        publish_thread_->join();
        publish_thread_.reset();
    }
    printf("stream pushlisher stopped\n");
}

void StreamPublisher::onPacketSent(size_t packet_size){
    if(!is_active_) return;
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto now = std::chrono::high_resolution_clock::now();
    last_packet_time_ = now;
    if(packet_size > 0 ){
        stats_.bytes_sent +=packet_size;
        stats_.connection_alive = true;
    }else{
        stats_.connection_alive = false;
        printf("Packet send failed: %s stream, size: %zu\n", packet_size);
    }

}
    

void  StreamPublisher::publishingLoop(){
    auto last_stats_time = std::chrono::high_resolution_clock::now();
    printf("StreamPublisher monitoring loop started\n");
    uint64_t last_bytes_sent = 0;

    while(!should_stop_){
        try{
            auto now = std::chrono::high_resolution_clock::now();

            bool connection_healthy = isConnectionHealthy();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_stats_time);

            if(elapsed.count() >= 1000){
                updateRealTimeStats();
                last_stats_time = now;
            }
            auto packet_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_packet_time_);
            if (packet_elapsed.count() > 10) { 
                // printf("StreamPublisher: No packets received for %ld seconds\n", packet_elapsed.count());
                
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.connection_alive = false;
            }

            if(!connection_healthy&& is_active_){
                if(reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS){
                    // printf("StreamPublisher:Triggering reconnection attempt\n");
                    reconnect();
                }
            }
        }catch(const std::exception& e){
            fprintf(stderr, "StreamPublisher monitoring error: %s\n", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    printf("StreamPublisher monitoring loop ended\n");
}
bool  StreamPublisher::reconnect(){
    //....
    return true;
}

bool StreamPublisher::isConnectionHealthy() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    bool healthy = stats_.connection_alive;
    return healthy;
}

void StreamPublisher::updateRealTimeStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    uint64_t bytes_diff = stats_.bytes_sent - last_total_bytes_;
    stats_.bitrate_kbps = (bytes_diff * 8.0) / 1000.0; 
    last_total_bytes_ = stats_.bytes_sent;
    if (bytes_diff > 0) {
        printf("StreamPublisher Stats:  Bitrate=%.2f kbps, Connected=%s\n",
            stats_.bitrate_kbps,stats_.connection_alive ? "Yes" : "No");
    }
}