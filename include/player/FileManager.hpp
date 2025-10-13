#include <string>
#include "BaseDecoder.hpp"
#include "frameWrite.hpp"
#include "Demuxer.hpp"
#include "AudioFilter.hpp"
#include "VideoFilter.hpp"
#include "FilterParams.hpp"
#include <optional>

struct AudioFilterParamUpdate{
    std::optional<double> volume;
    std::optional<double> tempo;
    std::optional<bool> enable_echo;
    std::optional<double> echo_in_delay;
    std::optional<double> echo_in_decay;
    std::optional<double> echo_out_delay;
    std::optional<double> echo_out_decay;
    std::optional<bool> enable_lowpass;
    std::optional<double> lowpass_freq;
    std::optional<bool> enable_highpass;
    std::optional<double> highpass_freq;
    std::optional<bool> mute;
};

struct VideoFilterParamsUpdate{
    std::optional<float> brightness;
    std::optional<float> contrast;
    std::optional<float> saturation;
    std::optional<int> rotate;
    std::optional<float> hue;
    std::optional<bool> enable_grayscale;
    std::optional<bool> enable_sepia;
    std::optional<int> blur_radius;
};

class FileManager {
public:
    FileManager();
    ~FileManager();
    void process_raw(const std::string& inputPath,
                     const std::string& outputPath,
                     MediaType mediaType,
                     BaseDecoder* decoder
                    );

    void process_mux(const std::string& inputPath,
                     const std::string& outputPath,
                     Demuxer* demuxer,
                     BaseDecoder* decoder_audio,
                     BaseDecoder* decoder_video);

    void updateFilterAudioParams(const AudioFilterParamUpdate& update);
    void updateFilterVideoParams(const VideoFilterParamsUpdate& update);

    void openAudioFilter();
    void closeAudioFilter();

    void openVideoFilter();
    void closeVideoFilter();


private:
    std::string buildOutputFilePath(const std::string& outputDir, const std::string& inputPath, const std::string& newSuffix);
    void processVideo(FILE* inputFile, const std::string& inputPath,
                               const std::string& outputDir, BaseDecoder* decoder,
                               AVPacket* pkt, uint8_t*& data, size_t& data_size);
    void processAudio(FILE* inputFile, const std::string& inputPath,const std::string& outputDir, BaseDecoder* decoder,AVPacket* pkt, uint8_t*& data, size_t& data_size);

    AudioFilterParams currentParamsAudio;
    VideoFilterParams currentParamsVideo;

    std::shared_ptr<AudioFilter> audioFilter = nullptr;
    std::shared_ptr<VideoFilter> videoFilter = nullptr;

    bool enableAudioFilter = false;
    bool enableVideoFilter = false;
    
    //audio format
    AVSampleFormat savedSampleFmt;
    uint64_t savedChannelLayout;
    int savedSampleRate;
    AVRational savedTimeBase;
    
    //video format
    int savedWidth;
    int savedHeight;
    AVPixelFormat pix_fmt;
    AVRational videoTimebase;
    AVRational sample_aspect_ratio;
};
