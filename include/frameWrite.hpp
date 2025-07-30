#ifndef FRAMEWRITER_HPP
#define FRAMEWRITER_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <memory>
#include <functional>
extern "C"{
    #include <libavutil/frame.h>
    #include <libavcodec/avcodec.h>
}

enum class MediaType { AUDIO, VIDEO };

class FrameWriter{
    public:
        virtual ~FrameWriter() = default;
        virtual bool open() = 0;
        virtual bool writeFrame(const AVFrame* frame) = 0;
        virtual void close() = 0;
};


class AudioExtraInfo {
    public:
        virtual ~AudioExtraInfo() = default;
        virtual void setAudioParams(int sampleRate ,int channels) = 0;
};

class PCMFrameWriter : public FrameWriter ,public AudioExtraInfo{
public:
    PCMFrameWriter(const std::string& filename);
    ~PCMFrameWriter();
    void setAudioParams(int sampleRates,int channels){
        this->channels_ = channels;
        this->bytesPerSample_ = sampleRates;
    }

    bool open() override;
    bool writeFrame(const AVFrame* frame) override;
    void close() override;

private:

    using WriteFunc = std::function<bool(const AVFrame*)>;
    WriteFunc writeImpl_;

    bool writePlanar(const AVFrame* frame);
    bool writePacked(const AVFrame* frame);
    bool initFromFrame(const AVFrame* frame);

    bool initialized_ = false;
    std::string filename_;
    FILE* outFile = nullptr;
    int bytesPerSample_ = 0;
    int channels_ = 0;
    int sampleRate_ = 0;
};

class YUVFrameWriter : public FrameWriter {
public:
    explicit YUVFrameWriter(const std::string& filename);
    ~YUVFrameWriter();

    bool open() override;
    bool writeFrame(const AVFrame* frame) override;
    void close() override;

private:
    std::string filename_;
    FILE* outFile = nullptr;
    int frameindex;

    //缓冲区相关
    uint8_t* video_dst_data[4] = {nullptr};
    int video_dst_linesize[4] = {0};
    int video_dst_bufsize = 0;
    int width = 0;
    int height = 0;
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
};

class FrameWriterFactory {
public:
    static std::unique_ptr<FrameWriter> createWriter(const std::string& filename, MediaType type) {
        if (type == MediaType::AUDIO) {
            return std::make_unique<PCMFrameWriter>(filename);
        } else if (type == MediaType::VIDEO) {
            return std::make_unique<YUVFrameWriter>(filename);
        }
        return nullptr;
    }
};
#endif