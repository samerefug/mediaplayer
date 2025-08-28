#ifndef DATASOURCE
#define DATASOURCE

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <fstream>
#include <thread>
#include "FrameBuffer.hpp"
#include "FormatConverter.hpp"
extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}
class MediaDataSource{
public:
    MediaDataSource():is_active_(false),data_manager_(nullptr){}
    virtual ~MediaDataSource() = default;
    void setDataManager(MediaDataManager* manager){
        data_manager_ = manager;
    }

    void setFormatConverter(MediaFormatConverter* converter){
        format_converter_ = converter;
    }

    virtual bool open() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
    virtual bool isActive() const { return is_active_;}

    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;


protected:
    bool is_active_;
    MediaDataManager* data_manager_;
    MediaFormatConverter* format_converter_; 
};


class RawFileDataSource :public MediaDataSource{
public:
    enum FileType{
        PCM_FILE,
        YUV_FILE
    };

    RawFileDataSource(const std::string& file_path,FileType type);
    virtual ~RawFileDataSource();

    void setPCMParams(int sample_rate,int channels,AVSampleFormat format,int samples_per_frame);
    void setYUVParams(int width ,int height,AVPixelFormat format ,int fps);

    bool open() override;
    bool start() override;
    void stop() override;
    void close() override;

    bool hasVideo() const override { return file_type_ == YUV_FILE; }
    bool hasAudio() const override { return file_type_ == PCM_FILE; }

private:
    void readPCMLoop();
    void readYUVLoop();

    std::string file_path_;
    FileType file_type_;
    std::ifstream file_stream_;

    int pcm_sample_rate_;
    int pcm_channels_;
    AVSampleFormat pcm_format_;
    int pcm_samples_per_frame_;
    int pcm_bytes_per_sample_;
    
    // YUV参数
    int yuv_width_;
    int yuv_height_;
    AVPixelFormat yuv_format_;
    int yuv_fps_;
    size_t yuv_frame_size_;
    
    std::unique_ptr<std::thread> read_thread_;

};

#endif