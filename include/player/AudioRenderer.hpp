#ifndef AUDIORENDERER
#define AUDIORENDERER

#include <SDL2/SDL.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
extern "C"{
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
}

class AudioRenderer {
public:
    virtual ~AudioRenderer() = default;
    virtual bool initialize(int sample_rate, int channels, AVSampleFormat format) = 0;
    virtual bool playFrame(AVFrame* frame) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void setVolume(float volume) = 0;
    virtual int getQueuedFrames() = 0;
    virtual void close() = 0;
    
    virtual void setMute(bool mute) = 0;
    virtual float getVolume() const = 0;
    virtual bool isMuted() const = 0;

    virtual void setTimeBase(AVRational time_base) = 0;
    virtual void setClockUpdateCallback(std::function<void(double)> callback) = 0;

    struct AudioStats {
        uint64_t frames_played = 0;
        uint64_t frames_dropped = 0;
        uint64_t total_samples = 0;
        int buffer_level = 0;    
        double latency_ms = 0.0;       
        bool underrun = false;         
    };
    virtual AudioStats getStats() const = 0;


};

class SDLAudioRenderer:public AudioRenderer{
public:
    SDLAudioRenderer();
    ~SDLAudioRenderer();

    bool initialize(int sample_rate, int channels, AVSampleFormat format) override;
    bool playFrame(AVFrame* frame) override;
    void pause() override;
    void resume() override;
    void setVolume(float volume) override;
    int getQueuedFrames() override;
    void close() override;
    
    void setMute(bool mute) override;
    float getVolume() const override;
    bool isMuted() const override;
    AudioStats getStats() const override;

    void setTimeBase(AVRational time_base) { time_base_ = time_base; }
    void setClockUpdateCallback(std::function<void(double)> callback) {
        clock_update_callback_ = callback;
    }

private:
    static void audioCallback(void* userdata,uint8_t* stream,int len);
    void fillAudioBuffer(uint8_t* stream,int len);
    void logAudioCallbackStats(int len, long long time_diff_us, int callback_count);
    struct AudioBuffer{
        std::vector<uint8_t> data;
        size_t read_pos = 0;
        double pts = AV_NOPTS_VALUE;
        int sample_count = 0;
        bool isEmpty() const {return read_pos >= data.size();}
        size_t availableBytes() const { return data.size() - read_pos;}
        void reset() {read_pos = 0; data.clear();}
    };

    bool convertFrameToBuffer(AVFrame* frame,AudioBuffer& buffer);
    SDL_AudioFormat avFormatToSDL(AVSampleFormat format);
    size_t getBytesPerSample(AVSampleFormat format ,int channels);

    SDL_AudioDeviceID audio_device_;
    SDL_AudioSpec audio_spec_;
    bool sdl_initialized_;
    bool device_opened_;

    int sample_rate_;
    int channels_;
    AVSampleFormat input_format_;
    int bytes_per_sample_;

    std::queue<AudioBuffer> buffer_queue_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    AudioBuffer current_buffer_;

    std::atomic<bool> is_paused_;
    std::atomic<bool> is_muted_;
    std::atomic<bool> is_stoped_;
    std::atomic<float> volume_;

    std::function<void(double)> clock_update_callback_;
    
    mutable std::mutex stats_mutex_;
    AudioStats stats_;
    std::chrono::high_resolution_clock::time_point last_callback_time_;

    static const size_t MAX_FRAME_QUEUE_SIZE = 10;
    static const int BUFFER_SIZE_MS = 100;

    AVRational time_base_;
    bool enable_debug;
};

#endif