#ifndef VIDEORENDERER
#define VIDEORENDERER
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <mutex>
extern "C"{
    #include <libavutil/frame.h>
    #include <libavutil/pixfmt.h>
    #include <libavutil/pixdesc.h>
}
class VideoRenderer {
public:
    virtual ~VideoRenderer() = default;
    virtual bool initialize(int width, int height, AVPixelFormat format) = 0;
    virtual bool displayFrame(AVFrame* frame) = 0;
    virtual void setDisplaySize(int width, int height) = 0;
    virtual void* getWindowHandle() = 0;
    virtual void close() =0;
    struct RenderStats {
        uint64_t frames_rendered = 0;
        uint64_t frames_dropped = 0;
        double average_fps = 0.0;
        int64_t render_time_us = 0;
    };
    virtual RenderStats getStats() const = 0;

};


class SDLVideoRenderer : public VideoRenderer {
public:
    SDLVideoRenderer();
    ~SDLVideoRenderer();
    
    bool initialize(int width, int height, AVPixelFormat format) override;
    
    bool displayFrame(AVFrame* frame) override;
    void setDisplaySize(int width, int height) override;
    void* getWindowHandle() override;


    Uint32 avPixelFormatToSDL(AVPixelFormat format);
    bool isFormatSupported(AVPixelFormat format);
    void close();
    VideoRenderer::RenderStats getStats() const;
private:

    bool createWindow();
    bool createRenderer();
    bool createTexture();
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    
    int video_width_, video_height_;
    int display_width_, display_height_;
    AVPixelFormat input_format_;

    bool sdl_initialized_;
    bool window_created_;
    bool renderer_created_;
    bool texture_created_;

    mutable std::mutex stats_mutex_;
    RenderStats stats_;
    std::chrono::high_resolution_clock::time_point last_stats_time_;
    uint64_t frame_count_since_last_stats_;
    bool enable_debug;
};

#endif