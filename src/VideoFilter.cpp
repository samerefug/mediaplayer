#include "VideoFilter.hpp"
#include <sstream>
#include <iomanip>

static std::string generate_filter_desc(const VideoFilterParams& params) {
    std::ostringstream oss;
    bool first = true;
    auto append_filter = [&](const std::string& f) {
        if (!first) oss << ",";
        oss << f;
        first = false;
    };

    // 亮度调整（Y通道）
    if (params.brightness != 0.0f) {
        float brightness_val = params.brightness * 255.0f;
        append_filter("lut=y='val+" + std::to_string(brightness_val) + "':u='val':v='val'");
    }

    // 饱和度调整（用 colorchannelmixer）
    if (params.saturation != 1.0f) {
        // 饱和度调节，简单线性缩放色彩
        float s = params.saturation;
        append_filter("colorchannelmixer=rr=" + std::to_string(0.393*s) + ":rg=" + std::to_string(0.769*s) + ":rb=" + std::to_string(0.189*s) +
                      ":gr=0:gg=" + std::to_string(s) + ":gb=0:" +
                      "br=0:bg=0:bb=" + std::to_string(s));
    }

    // 旋转（保持你之前的实现）
    if (params.rotate == 90) {
        append_filter("transpose=1");
    } else if (params.rotate == 180) {
        append_filter("transpose=2,transpose=2");
    } else if (params.rotate == 270) {
        append_filter("transpose=2,transpose=1");
    }

    // 色调
    if (params.hue != 0.0f) {
        append_filter("hue=h=" + std::to_string(params.hue));
    }

    if (params.enable_grayscale) {
        append_filter("hue=s=0");
    }

    if (params.enable_sepia) {
        append_filter("colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131");
    }

    if (params.blur_radius > 0) {
        append_filter("gblur=sigma=" + std::to_string(params.blur_radius));
    }

    if (first) {
        return "null";
    }
    return oss.str();
}


void VideoFilter::init_graph(const VideoFilterParams& params,
                            int width, int height,
                            AVPixelFormat pix_fmt,
                            AVRational time_base,
                            AVRational sample_aspect_ratio){
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();

    current_desc_ = generate_filter_desc(params);
    graph_ = avfilter_graph_alloc();
    if (!graph_||!outputs||!inputs) {
        fprintf(stderr,"Failed to allocate filter graph");
        exit(1);
    }


    
    ret = snprintf(args,sizeof(args),
                    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                width,height,pix_fmt, time_base.num,time_base.den,
                     sample_aspect_ratio.num, sample_aspect_ratio.den
                    );

    ret = avfilter_graph_create_filter(&src_ctx_,buffersrc,"in",args,nullptr,graph_);
    if(ret<0){
        fprintf(stderr,"create_filter in failed");
        exit(1);
    }
    ret = avfilter_graph_create_filter(&sink_ctx_,buffersink,"out",nullptr,nullptr,graph_);
    if(ret<0){
         fprintf(stderr,"create_filter out failed");
         exit(1);
    }


    std::string full_desc = current_desc_.empty()? "null" :current_desc_;

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = src_ctx_;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
 

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = sink_ctx_;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    ret = avfilter_graph_parse_ptr(graph_, full_desc.c_str(), &inputs, &outputs, nullptr);
    if (ret < 0) {
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        fprintf(stderr, "Error parsing video filter graph '%s'\n", full_desc.c_str());
        exit(1);
    }

    ret = avfilter_graph_config(graph_, nullptr);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if (ret < 0) {
        fprintf(stderr, "Error config filter");
        exit(1);
    }
}

VideoFilter::VideoFilter(const VideoFilterParams& params,
                        int width, int height,
                        AVPixelFormat pix_fmt,
                        AVRational time_base,
                        AVRational sample_aspect_ratio
                        ){
init_graph(params,width,height,pix_fmt,time_base,sample_aspect_ratio);
}

VideoFilter::~VideoFilter() {
    cleanup();
}

void VideoFilter::reset(const VideoFilterParams& params,
                        int width, int height,
                        AVPixelFormat pix_fmt,
                        AVRational time_base,
                        AVRational sample_aspect_ratio){
    cleanup();
    init_graph(params,width,height,pix_fmt,time_base,sample_aspect_ratio);
}

void VideoFilter::cleanup(){
    if(graph_){
    avfilter_graph_free(&graph_);
    }
    src_ctx_ = nullptr;
    sink_ctx_ = nullptr;
}

void VideoFilter::push_frame(AVFrame* frame) {
    if (!src_ctx_)  fprintf(stderr,"Filter graph not initialized");
    int ret = av_buffersrc_add_frame_flags(src_ctx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        fprintf(stderr,"Failed to feed video frame into filter");
        exit(1);
    }
}

AVFrame* VideoFilter::pull_frame() {
    if (!sink_ctx_){
        fprintf(stderr,"Filter graph not initialized");
        exit(1);
    }

    AVFrame* filt = av_frame_alloc();
    if (!filt) {
        fprintf(stderr,"Failed to allocate video output frame");
        exit(1);
    }
    int ret = av_buffersink_get_frame(sink_ctx_, filt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&filt);
        return nullptr;
    } else if (ret < 0) {
        av_frame_free(&filt);
        fprintf(stderr,"Error pulling filtered video frame");
    }
    return filt;
}