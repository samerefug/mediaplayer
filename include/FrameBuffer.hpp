#ifndef FRAMEBUFFER
#define FRAMEBUFFER

#include <memory>
#include <vector>
#include <atomic>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}

class VideoFrameBuffer{
public:
    VideoFrameBuffer(int width,int height,AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P);
    ~VideoFrameBuffer();

    bool addFrame(const uint8_t* frame_data,size_t data_size);
    bool addFrame(AVFrame*frame);

    bool getFrame(int frame_index, uint8_t*& frame_data);
    AVFrame* getAVFrame(int frame_index);

    int getFrameCount() const {return frame_count_;}
    void clear();
private:
    int width_;
    int height_;
    int frame_count_;
    size_t frame_size_;
    std::vector<uint8_t> buffer_;
    AVPixelFormat pix_fmt_;

};

class AudioFrameBuffer{
public:
    AudioFrameBuffer(int sample_rate ,int channels, AVSampleFormat sample_fmt);
    ~AudioFrameBuffer();

    bool getSamples(int start_sample,int nb_samples,uint8_t*& sample_data);
    AVFrame* getAVFrame(int start_sample,int nb_samples);

    bool addFrame(const uint8_t* sample_data,int nb_samples);
    bool addFrame(AVFrame* frame);

    void clear();

private:
    int nb_samples_;
    int sample_rate_;
    int channels_;
    AVSampleFormat sample_fmt_;
    int bytes_per_sample_;
    std::vector<uint8_t> buffer_;
    int total_samples_;
};

class MediaDataManager {
public:
    MediaDataManager() = default;
    ~MediaDataManager() = default;

    bool initVideoBuffer(int width, int height, AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P);
    bool initAudioBuffer(int sample_rate, int channels, AVSampleFormat sample_fmt = AV_SAMPLE_FMT_S32);

    VideoFrameBuffer* getVideoBuffer() { return video_buffer_.get(); }
    AudioFrameBuffer* getAudioBuffer() { return audio_buffer_.get(); }

    bool hasVideo() const { return video_buffer_ != nullptr; }
    bool hasAudio() const { return audio_buffer_ != nullptr; }

    void clearAll();

private:
    std::unique_ptr<VideoFrameBuffer> video_buffer_;
    std::unique_ptr<AudioFrameBuffer> audio_buffer_;

};
#endif