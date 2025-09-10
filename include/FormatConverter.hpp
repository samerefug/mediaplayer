#ifndef FORMATCONVERTER
#define FORMATCONVERTER

extern "C"{
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/frame.h>
}

class MediaFormatConverter{
public:
    MediaFormatConverter();
    ~MediaFormatConverter();
    bool initVideoConverter(int src_width ,int src_height,AVPixelFormat& src_format,
                            int dst_width ,int dst_height,AVPixelFormat& dst_format);

    AVFrame* convertVideo(AVFrame* src_frame);
    bool initAudioConverter(AVSampleFormat src_format, int src_sample_rate, AVChannelLayout&  src_layout,
                           AVSampleFormat dst_format, int dst_sample_rate, AVChannelLayout& dst_layout);

    AVFrame* convertAudio(AVFrame* src_frame);

    bool needVideoConversion() const { return video_converter_initialized_; }
    bool needAudioConversion() const { return audio_converter_initialized_; }
    
    void cleanup();
private:
    SwsContext* sws_ctx_;
    AVFrame* converted_video_frame_;
    bool video_converter_initialized_;
    
    int dst_video_width_, dst_video_height_;
    AVPixelFormat dst_video_format_;
    
    SwrContext* swr_ctx_;
    AVFrame* converted_audio_frame_;
    bool audio_converter_initialized_;
    
    AVSampleFormat dst_audio_format_;
    int dst_audio_sample_rate_;
    AVChannelLayout dst_audio_layout_ ={};
    int max_dst_samples_;
    
    bool allocateVideoFrame();
    bool allocateAudioFrame(int nb_samples);
};

#endif