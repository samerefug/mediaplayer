#ifndef FRAMEWRITER_HPP
#define FRAMEWRITER_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <memory>
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
        this->sampleRate_ = sampleRates;
    }

    bool open() override;
    bool writeFrame(const AVFrame* frame) override;
    void close() override;


private:
    std::string filename_;
    FILE* outFile = nullptr;
    int sampleRate_ = 0;
    int channels_ = 0;
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