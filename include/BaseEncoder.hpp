#ifndef BASEENCODER
#define BASEENCODER

#include <functional>
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
}

class BaseEncoder {
public:
    using PacketCallback = std::function<int(AVPacket*)>;

    BaseEncoder() = default;
    virtual ~BaseEncoder() {}

    virtual bool init(const char* name = nullptr, AVCodecID id = AV_CODEC_ID_NONE ,AVDictionary* opt_arg =nullptr) = 0;

    virtual void close() = 0;

    virtual int encode(AVFrame* encode_frame, PacketCallback callback) = 0;

    AVCodecContext* getCodecContext() const { return c; }
protected:
    const AVCodec* codec = nullptr;
    AVCodecContext* c = nullptr;
};

#endif