#include "player/VideoRenderer.hpp"
#include <chrono>


/*    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    
    int video_width_, video_height_;
    int display_width_, display_height_;
    AVPixelFormat input_format_;
*/


void logVideoCallbackStats(AVFrame* frame){
    static int debug_frame_count = 0;

    if(debug_frame_count < 10 && frame->format == AV_PIX_FMT_YUV420P){
        fprintf(stderr, "Frame %d: width=%d, height=%d, format=%d, linesize=[%d,%d,%d]\n",
        debug_frame_count,
        frame->width, frame->height, frame->format,
        frame->linesize[0], frame->linesize[1], frame->linesize[2]);
        
        char filename[256];
        snprintf(filename, sizeof(filename), "frame_%05d.ppm", debug_frame_count);
        FILE* f = fopen(filename, "wb");
        if(f){
            fprintf(f, "P6\n%d %d\n255\n", frame->width, frame->height);
            for(int y=0; y<frame->height; y++){
                for(int x=0; x<frame->width; x++){
                    fputc(frame->data[0][y * frame->linesize[0] + x], f); // Y
                    fputc(frame->data[1][y/2 * frame->linesize[1] + x/2], f); // U
                    fputc(frame->data[2][y/2 * frame->linesize[2] + x/2], f); // V
                }
            }
            fclose(f);
        }
    }
    debug_frame_count++;
}

SDLVideoRenderer::SDLVideoRenderer()
    :window_(nullptr)
    ,renderer_(nullptr)
    ,texture_(nullptr)
    ,video_width_(0)
    ,video_height_(0)
    ,display_width_(800)
    ,display_height_(600)
    ,input_format_(AV_PIX_FMT_NONE){

}


SDLVideoRenderer::~SDLVideoRenderer() {
}

VideoRenderer::RenderStats SDLVideoRenderer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}


bool SDLVideoRenderer::initialize(int width, int height, AVPixelFormat format){
    video_height_ = height;
    video_width_ = width;
    input_format_ = format;

     if (!isFormatSupported(format)) {
        printf("Unsupported pixel format: %s\n", av_get_pix_fmt_name(format));
        return false;
    }

    if(!sdl_initialized_){
         if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            printf("SDL init failed: %s\n", SDL_GetError());
            return false;
        }
        sdl_initialized_ = true;
    }
    
    if(!createWindow()){
        return false;
    }

    if (!createRenderer()) {
        return false;
    }
    
    if (!createTexture()) {
        return false;
    }
    
    printf("SDL Video Renderer initialized: %dx%d, format: %s\n",
           width, height, av_get_pix_fmt_name(format));
    
    return true;
}

void SDLVideoRenderer::setDisplaySize(int width, int height){
    display_width_ = width;
    display_height_ = height;
    
    if (window_) {
        SDL_SetWindowSize(window_, width, height);
    }
}



bool SDLVideoRenderer::createWindow(){
    if(window_created_){
        return true;
    }

    window_ = SDL_CreateWindow(
        "Media Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        display_width_,
        display_height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if(!window_){
        fprintf(stderr,"fail to create window:%s\n", SDL_GetError());
        return false;
    }

    window_created_ = true;
    return true;
}

bool SDLVideoRenderer::createRenderer(){
    if(renderer_created_ || !window_){
        return renderer_created_;
    }

    renderer_ = SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(!renderer_){
        fprintf(stderr,"failed to create SDL renderer: %s\n",SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer_,0,0,0,255);
    renderer_created_ = true;
    return true;
}

bool SDLVideoRenderer::createTexture(){
    if(texture_created_ || !renderer_ ){
        return texture_created_;
    }
    Uint32 sdl_format = avPixelFormatToSDL(input_format_);
    if(sdl_format == SDL_PIXELFORMAT_UNKNOWN){
        printf("cannot convert AVPixelFormat to SDL format\n");
        return false;
    }
    
    texture_ = SDL_CreateTexture(
        renderer_,
        sdl_format,
        SDL_TEXTUREACCESS_STREAMING,
        video_width_,
        video_height_
    );

    if(!texture_){
        fprintf(stderr,"failed to create SDL texture%s\n" ,SDL_GetError());
        return false;
    }

    texture_created_ = true;
    return true;
}
Uint32 SDLVideoRenderer::avPixelFormatToSDL(AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_YUV420P:
            return SDL_PIXELFORMAT_IYUV;
        case AV_PIX_FMT_RGB24:
            return SDL_PIXELFORMAT_RGB24;
        case AV_PIX_FMT_BGR24:
            return SDL_PIXELFORMAT_BGR24;
        case AV_PIX_FMT_RGB32:
            return SDL_PIXELFORMAT_RGBA32;
        case AV_PIX_FMT_BGR32:
            return SDL_PIXELFORMAT_BGRA32;
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

bool SDLVideoRenderer::isFormatSupported(AVPixelFormat format) {
    return avPixelFormatToSDL(format) != SDL_PIXELFORMAT_UNKNOWN;
}

bool SDLVideoRenderer::displayFrame(AVFrame* frame) {
    if (!frame || !texture_ || !renderer_) {
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    
    if(enable_debug){ 
        logVideoCallbackStats(frame);
    }
    if (input_format_ == AV_PIX_FMT_YUV420P) {
        if (SDL_UpdateYUVTexture(
                texture_,
                nullptr,
                frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]) < 0) {
            fprintf(stderr, "SDL_UpdateYUVTexture failed: %s\n", SDL_GetError());
            return false;
        }
    } else {
        void* pixels;
        int pitch;
        if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) < 0) {
            fprintf(stderr, "failed to lock texture: %s\n", SDL_GetError());
            return false;
        }
        for (int y = 0; y < video_height_; y++) {
            memcpy((uint8_t*)pixels + y * pitch,
                   frame->data[0] + y * frame->linesize[0],
                   std::min(pitch, frame->linesize[0]));
        }
        SDL_UnlockTexture(texture_);
    }

    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto render_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_rendered++;
        stats_.render_time_us = render_time.count();
        frame_count_since_last_stats_++;
    }

    return true;
}

void SDLVideoRenderer::close() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        texture_created_ = false;
    }
    
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
        renderer_created_ = false;
    }
    
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        window_created_ = false;
    }
    
    if (sdl_initialized_) {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}


 void* SDLVideoRenderer::getWindowHandle() {
    return (void*)window_;
 }