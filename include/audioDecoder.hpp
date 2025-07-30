#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "baseDecoder.hpp"
extern "C"{
    #include <libavutil/frame.h>
    #include <libavutil/mem.h>
    #include <libavcodec/avcodec.h>
}

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096


class AudioDecoder :public BaseDecoder{
    public:
        AudioDecoder();
        ~AudioDecoder();

        bool initialize(AVCodecID codec_id);
        bool flush();
        bool parsePacket(uint8_t *&inbuf, size_t& size,AVPacket* pkt);
        bool sendPacketAndReceiveFrame(AVPacket* pkt);
        AVFrame *getFrame() const;
        AVSampleFormat getSampleFormat() const;
        int get_bytes_per_sample()const;
        int get_channels()const;

    private:
        enum AVSampleFormat sfmt;
};
