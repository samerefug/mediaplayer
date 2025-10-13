#include "AudioFilter.hpp"
#include <sstream>
#include <iomanip>




static std::string generate_filter_desc(const AudioFilterParams& params){
    std::ostringstream oss;
    oss<< std::fixed<<std::setprecision(2);

    bool first = true;
    auto append_filter = [&](const std::string& f){
        if(!first) oss<<",";
        oss<<f;
        first = false;
    };

    if(params.volume !=1.0){
        oss<<std::fixed<<std::setprecision(2);
        append_filter("volume=" + std::to_string(params.volume));
    }

    if(params.tempo != 1.0){
        append_filter("atempo=" + std::to_string(params.tempo));
    }

    if(params.enable_echo){
        oss << std::fixed<<std::setprecision(1);
        append_filter("aecho=" + std::to_string(params.echo_in_delay) + ":" + 
                                 std::to_string(params.echo_in_decay) + ":" +
                                 std::to_string(params.echo_out_delay) + ":" +
                                 std::to_string(params.echo_out_decay)  
                                );
    }
    
    if(params.enable_lowpass && params.lowpass_freq > 0.0){
        append_filter("lowpass=f=" + std::to_string(params.lowpass_freq));
    }
    if(params.enable_highpass && params.highpass_freq > 0.0){
        append_filter("highpass=f=" + std::to_string(params.lowpass_freq));
    }

    if(first){
        return "anull";
    }
    return oss.str();
    }
    

AudioFilter::AudioFilter(const AudioFilterParams& params,
                        AVSampleFormat sample_fmt,
                        uint64_t channel_layout,
                        int sample_rate,
                        AVRational time_base){
init_graph(params, sample_fmt, channel_layout, sample_rate, time_base);
}

AudioFilter::~AudioFilter() {
    cleanup();
}

void AudioFilter::init_graph(const AudioFilterParams& params,
                        AVSampleFormat sample_fmt,
                        uint64_t channel_layout,
                        int sample_rate,
                        AVRational time_base){
    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();

    current_desc_ = generate_filter_desc(params);
    graph_ = avfilter_graph_alloc();
    if (!graph_||!outputs||!inputs) {
        fprintf(stderr,"Failed to allocate filter graph");
        exit(1);
    }


    ret = snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
             time_base.num, time_base.den, sample_rate,av_get_sample_fmt_name(sample_fmt),(unsigned long long)channel_layout);
    

    ret = avfilter_graph_create_filter(&src_ctx_,abuffersrc,"in",args,nullptr,graph_);
    if(ret<0){
        fprintf(stderr,"create_filter in failed");
        exit(1);
    }
    ret = avfilter_graph_create_filter(&sink_ctx_,abuffersink,"out",nullptr,nullptr,graph_);
    if(ret<0){
         fprintf(stderr,"create_filter out failed");
         exit(1);
    }
    //暂时不约束输出格式
    std::string full_desc = current_desc_.empty()? "anull" :current_desc_;

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
        fprintf(stderr, "Error parsing audio filter graph '%s'\n", full_desc.c_str());
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

void AudioFilter::reset(const AudioFilterParams& params,
                        AVSampleFormat sample_fmt,
                        uint64_t channel_layout,
                        int sample_rate,
                        AVRational time_base){
    cleanup();
    init_graph(params,sample_fmt,channel_layout,sample_rate,time_base);
}

void AudioFilter::cleanup(){
    if(graph_){
    avfilter_graph_free(&graph_);
    }
    src_ctx_ = nullptr;
    sink_ctx_ = nullptr;
}

void AudioFilter::push_frame(AVFrame* frame) {
    if (!src_ctx_)  fprintf(stderr,"Filter graph not initialized");
    int ret = av_buffersrc_add_frame_flags(src_ctx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        fprintf(stderr,"Failed to feed audio frame into filter");
        exit(1);
    }
}

AVFrame* AudioFilter::pull_frame() {
    if (!sink_ctx_){
        fprintf(stderr,"Filter graph not initialized");
        exit(1);
    }

    AVFrame* filt = av_frame_alloc();
    if (!filt) {
        fprintf(stderr,"Failed to allocate audio output frame");
        exit(1);
    }
    int ret = av_buffersink_get_frame(sink_ctx_, filt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&filt);
        return nullptr;
    } else if (ret < 0) {
        av_frame_free(&filt);
        fprintf(stderr,"Error pulling filtered audio frame");
    }
    return filt;
}