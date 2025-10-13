#ifndef BASEENCODER
#define BASEENCODER

#include <functional>
#include <queue>
#include <mutex>
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
}

class BaseEncoder {
public:

    BaseEncoder() = default;
    virtual ~BaseEncoder() = default;

    virtual bool init(const char* name = nullptr, AVCodecID id = AV_CODEC_ID_NONE ,AVDictionary* opt_arg =nullptr) = 0;

    virtual int flush() { return encode(nullptr);}
    virtual void close() = 0;

    virtual int encode(AVFrame* encode_frame) = 0;
    AVPacket* getEncodedPacket(){
        std::lock_guard<std::mutex> lock(packet_mutex);
        if(packet_queue_.empty()){
            return nullptr;
        }

        AVPacket*pkt = packet_queue_.front();
        packet_queue_.pop();
        return pkt;
    }

    AVCodecContext* getCodecContext() const { return c; }

    AVCodecParameters* getCodecParameters() const {
        if (!c) return nullptr;
        
        AVCodecParameters* par = avcodec_parameters_alloc();
        if (par) {
            avcodec_parameters_from_context(par, c);
        }
        return par;
    }
protected:
    const AVCodec* codec = nullptr;
    AVCodecContext* c = nullptr;
    std::queue<AVPacket*> packet_queue_;
    std::mutex packet_mutex;
};

#endif