// #ifndef STREAMPUSHER
// #define STREAMPUSHER

// #include <string>
// extern "C"{
// #include <libavformat/avformat.h>   // AVFormatContext、封装、IO
// #include <libavcodec/avcodec.h>     // 编码/解码器
// #include <libavutil/avutil.h>       // AVPacket、时间戳工具
// #include <libavutil/mathematics.h>  // 时间戳计算
// #include <libavutil/opt.h>          // 编码器参数设置
// #include <libavutil/imgutils.h>     // 图像缓冲区工具
// #include <libavutil/samplefmt.h>    // 音频格式
// #include <libswscale/swscale.h>     // 视频像素格式转换
// #include <libswresample/swresample.h> // 音频重采样
// }


// class StreamPusher{
//     public:
//     StreamPusher(const std::string& rtmp_url);
//     ~StreamPusher();

//     bool init(int width,int height,int fps, int sample_rate,int channels);
//     bool pushVideoFrame(uint8_t* yuv_data);
//     bool pushAudioFrame(uint8_t* pcm_data,int nb_samples);
//     void close();

//     private:
//     AVFormatContext* ofmt_ctx = nullptr;
//     AVStream* video_st = nullptr;
//     AVStream* audio_st = nullptr;
//     AVCodecContext* video_codec_ctx = nullptr;
//     AVCodecContext* audio_codec_ctx = nullptr;

//     int video_pts = 0;
//     int audio_pts = 0;
//     std::string rtmp_url;

//     bool initVideoStream(int width, int height, int fps);
//     bool initAudioStream(int sample_rate, int channels);
//     AVPacket* encodeVideoFrame(uint8_t* yuv_data);
//     AVPacket* encodeAudioFrame(uint8_t* pcm_data, int nb_samples);
// };

// #endif