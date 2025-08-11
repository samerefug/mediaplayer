#ifndef AUDIOFILTER
#define AUDIOFILTER

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include"FilterParams.hpp"
class AudioFilter {
    public:
        AudioFilter(const AudioFilterParams& params,
                    AVSampleFormat sample_fmt_,
                    uint64_t channel_layout,
                    int sample_rate,
                    AVRational time_base);            
        ~AudioFilter();
        void push_frame(AVFrame* frame);
        AVFrame* pull_frame();
        void reset(const AudioFilterParams& params,
                    AVSampleFormat sample_fmt_,
                    uint64_t channel_layout,
                    int sample_rate,
                    AVRational time_base);
    private:
        
        void init_graph(const AudioFilterParams& params,
                        AVSampleFormat sample_fmt,
                        uint64_t channel_layout,
                        int sample_rate,
                        AVRational time_base);
        void cleanup();
        std::string current_desc_;
        AVFilterGraph* graph_ = nullptr;
        AVFilterContext* src_ctx_ = nullptr;
        AVFilterContext* sink_ctx_ = nullptr;
};

#endif