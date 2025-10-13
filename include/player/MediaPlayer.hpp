#ifndef MEDIAPLAYER
#define MEDIAPLAYER
#include<iostream>
#include<memory>
#include<functional>
#include<string>
#include "PlaybackCoordinator.hpp"
#include "VideoDecoder.hpp"
#include "AudioDecoder.hpp"
#include "Demuxer.hpp"


class MediaPlayer{
public:
    MediaPlayer();
    ~MediaPlayer();

    bool openFile(const std::string& file_path);
    bool openUrl(const std::string& url);
    void close();

    
    bool play();
    void pause();
    void run();
    void resume();
    void stop();
    bool seek(double time_seconds);

    bool hasVideo() const;
    bool hasAudio() const;

    enum PlayState{
        STOPPED,
        PLAYING,
        PAUSED,
        SEEKING,
        ERROR_STATE
    };
    void setVolume(float volume);

    Demuxer::MediaInfo getMediaInfo();

private:
    bool initializeComponents();
    void cleanupComponents();

    void renderVideoFrame();
    PlayState current_state_;
    std::string current_media_url_;

    std::mutex state_mutex_;
    
    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<AudioDecoder> audio_decoder_;
    std::unique_ptr<VideoDecoder> video_decoder_;
    std::unique_ptr<AudioRenderer> audio_renderer_;
    std::unique_ptr<VideoRenderer> video_renderer_;
    std::unique_ptr<PlaybackCoordinator> coordinator_;

    std::thread audio_thread_;
};


#endif