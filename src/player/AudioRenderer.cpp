#include "AudioRenderer.hpp"



void SDLAudioRenderer::logAudioCallbackStats(int len, long long time_diff_us, int callback_count) {
    if (callback_count % 100 != 0) {
        return;
    }

    long long expected_interval_us =
        (long long)(len * 1000000LL / (audio_spec_.freq * audio_spec_.channels * bytes_per_sample_));

    printf("音频回调统计 [第%d次]:\n", callback_count);
    printf("  回调间隔: %lld us (期望: %lld us)\n", 
           time_diff_us, expected_interval_us);
    printf("  请求字节: %d\n", len);
    printf("  队列大小: %zu\n", buffer_queue_.size());
    printf("  采样率: %d Hz\n", audio_spec_.freq);
    printf("  声道数: %d\n", audio_spec_.channels);
    printf("---------------------------------------------\n");
}

SDLAudioRenderer::~SDLAudioRenderer() {
}

SDLAudioRenderer::SDLAudioRenderer()
    : audio_device_(0)
    , sdl_initialized_(false)
    , device_opened_(false)
    , sample_rate_(0)
    , channels_(0)
    , input_format_(AV_SAMPLE_FMT_NONE)
    , bytes_per_sample_(0)
    , is_paused_(false)
    , is_muted_(false)
    , is_stoped_(false)
    , volume_(1.0f) {

    audio_spec_ = {};
    stats_ = {};
    last_callback_time_ = std::chrono::high_resolution_clock::now();
}
bool SDLAudioRenderer::initialize(int sample_rate,int channels, AVSampleFormat format){
    sample_rate_ = sample_rate;
    channels_ = channels;
    input_format_ = format;
    bytes_per_sample_ = getBytesPerSample(format,channels);

    if(!sdl_initialized_){
        if(SDL_Init(SDL_INIT_AUDIO) < 0){
            fprintf(stderr,"SDL audio init failed: %s\n",SDL_GetError());
            return false;
        }
    }

    sdl_initialized_ = true;

    SDL_AudioSpec desired_spec;
    memset(&desired_spec,0,sizeof(desired_spec));
    desired_spec.freq = sample_rate;
    desired_spec.format = avFormatToSDL(format);
    desired_spec.channels = channels;
    desired_spec.samples = 1024;
    desired_spec.callback = audioCallback;
    desired_spec.userdata = this;

    
    
    audio_device_ = SDL_OpenAudioDevice(
        nullptr,
        0,
        &desired_spec,
        &audio_spec_,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE
    );

    if(audio_device_ == 0){
        fprintf(stderr,"failed to open audio device%s\n",SDL_GetError());
        return false;
    }

    device_opened_ = true;

    printf("  SDL Audio Renderer initialized:\n");
    printf("  Sample Rate: %d Hz (requested: %d Hz)\n", audio_spec_.freq, sample_rate);
    printf("  Channels: %d (requested: %d)\n", audio_spec_.channels, channels);
    printf("  Format: 0x%x\n", audio_spec_.format);
    printf("  Buffer Size: %d samples\n", audio_spec_.samples);

    SDL_PauseAudioDevice(audio_device_, 0);
    printf("Start audio playback (device=%d)\n", audio_device_);
    return true;
}

bool SDLAudioRenderer::playFrame(AVFrame* frame){
    if (!frame || !device_opened_) {
        return false;
    }

    AudioBuffer buffer;
    if (!convertFrameToBuffer(frame, buffer)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_queue_.push(std::move(buffer));
        stats_.frames_played++;
        stats_.total_samples += frame->nb_samples;
    }

    buffer_cv_.notify_one();
    return true;
}

bool SDLAudioRenderer::convertFrameToBuffer(AVFrame* frame,AudioBuffer& buffer){
    if(!frame || frame->nb_samples<=0){
        return false;
    }

    size_t buffer_size = frame->nb_samples * channels_ * bytes_per_sample_;
    buffer.data.resize(buffer_size);


    buffer.read_pos = 0;
    buffer.pts = frame->pts;
    buffer.sample_count = frame->nb_samples;

    if(av_sample_fmt_is_planar(input_format_)){
        int sample_size = av_get_bytes_per_sample(input_format_);
        uint8_t* dst = buffer.data.data();
        for(int i = 0; i< frame->nb_samples;i++){
            for(int ch = 0;ch < channels_;ch++){
                memcpy(dst,frame->data[ch] + i * sample_size,sample_size);
                dst += sample_size;
            }
        }
    }else{
        memcpy(buffer.data.data(),frame->data[0],buffer_size);
    }



    return true;
}

void SDLAudioRenderer::audioCallback(void* userdata,uint8_t* stream ,int len){
    SDLAudioRenderer* renderer = static_cast<SDLAudioRenderer*>(userdata);
    renderer->fillAudioBuffer(stream,len);
}

void SDLAudioRenderer::fillAudioBuffer(uint8_t* stream, int len){
    auto callbackTime = std::chrono::high_resolution_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::microseconds>(callbackTime - last_callback_time_);
    last_callback_time_ = callbackTime;

    double played_pts = AV_NOPTS_VALUE;
    bool pts_captured = false;

    if(enable_debug){
        static int callback_count = 0;
        callback_count++;
        logAudioCallbackStats(len, time_diff.count(), callback_count);
    }
    memset(stream,audio_spec_.silence,len);
    if(is_paused_ || is_muted_ || is_stoped_){
        return;
    }

    int bytes_to_fill = len;
    uint8_t* output_ptr = stream;


    while(bytes_to_fill >0){
        if(current_buffer_.isEmpty()){
            std::unique_lock<std::mutex> lock(buffer_mutex_);

            if(buffer_queue_.empty()){
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.underrun = true;
                }
                break;
            }

            current_buffer_ = std::move(buffer_queue_.front());
            buffer_queue_.pop();

            if(!pts_captured && current_buffer_.pts != AV_NOPTS_VALUE){
                played_pts = current_buffer_.pts;
                pts_captured = true;
            }
        }

        size_t available = current_buffer_.availableBytes();
        size_t to_copy = std::min((size_t)bytes_to_fill,available);

        memcpy(output_ptr,current_buffer_.data.data() + current_buffer_.read_pos,to_copy);

        current_buffer_.read_pos +=to_copy;
        output_ptr += to_copy;
        bytes_to_fill -= to_copy;

        if(current_buffer_.isEmpty()){
            current_buffer_.reset();
        }
    }

    if (pts_captured && played_pts != AV_NOPTS_VALUE && clock_update_callback_) {
        double pts_seconds = played_pts * av_q2d(time_base_);
        clock_update_callback_(pts_seconds);

    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.buffer_level = (buffer_queue_.size() * 100) / MAX_FRAME_QUEUE_SIZE;
        stats_.latency_ms = (buffer_queue_.size() * BUFFER_SIZE_MS);
        stats_.underrun = (bytes_to_fill > 0);
    }
  
}

SDL_AudioFormat SDLAudioRenderer::avFormatToSDL(AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return AUDIO_U8;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return AUDIO_S16SYS;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return AUDIO_S32SYS;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return AUDIO_F32SYS;
        default:
            return AUDIO_F32SYS;
    }
}

size_t SDLAudioRenderer::getBytesPerSample(AVSampleFormat format, int channels) {
    int bytes_per_channel = av_get_bytes_per_sample(format);
    return bytes_per_channel;
}


void SDLAudioRenderer::pause() {
    if (device_opened_) {
        SDL_PauseAudioDevice(audio_device_, 1);
        is_paused_ = true;
    }
}

void SDLAudioRenderer::resume() {
    if (device_opened_) {
        SDL_PauseAudioDevice(audio_device_, 0);
        is_paused_ = false;
    }
}

void SDLAudioRenderer::setVolume(float volume) {
    volume_ = std::max(0.0f, std::min(1.0f, volume));
}

void SDLAudioRenderer::setMute(bool mute) {
    is_muted_ = mute;
}

float SDLAudioRenderer::getVolume() const {
    return volume_;
}

bool SDLAudioRenderer::isMuted() const {
    return is_muted_;
}

int SDLAudioRenderer::getQueuedFrames() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return buffer_queue_.size();
}

AudioRenderer::AudioStats SDLAudioRenderer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SDLAudioRenderer::close() {
    is_stoped_ = true;
    if (device_opened_) {
        SDL_PauseAudioDevice(audio_device_, 1);
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
        device_opened_ = false;
    }
    
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        while (!buffer_queue_.empty()) {
            buffer_queue_.pop();
        }
        current_buffer_.reset();
    }
    
    if (sdl_initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        sdl_initialized_ = false;
    }
}