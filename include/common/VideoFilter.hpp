#ifndef VIDEOFILTER
#define VIDEOFILTER

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

class VideoFilter {
    public:
        VideoFilter(const VideoFilterParams& params,
                        int width, int height,
                        AVPixelFormat pix_fmt,
                        AVRational time_base,
                        AVRational sample_aspect_ratio);
        ~VideoFilter();
        void push_frame(AVFrame* frame);
        AVFrame* pull_frame();
        void reset(const VideoFilterParams& params,
                        int width, int height,
                        AVPixelFormat pix_fmt,
                        AVRational time_base,
                        AVRational sample_aspect_ratio);
    private:
        void init_graph(const VideoFilterParams& params,
                        int width, int height,
                        AVPixelFormat pix_fmt,
                        AVRational time_base,
                        AVRational sample_aspect_ratio);
        void cleanup();
        std::string current_desc_;
        AVFilterGraph* graph_ = nullptr;
        AVFilterContext* src_ctx_ = nullptr;
        AVFilterContext* sink_ctx_ = nullptr;

};

#endif