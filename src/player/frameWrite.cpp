#include "frameWrite.hpp"
#include <iostream>
#include <cassert>

extern "C"{
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}


PCMFrameWriter::PCMFrameWriter(const std::string& filename)
    : filename_(filename),channels_(0),bytesPerSample_(0){}

PCMFrameWriter::~PCMFrameWriter() {
    close();
}

bool PCMFrameWriter::open() {
    outFile = fopen(filename_.c_str(), "wb");
    if (!outFile) {
        std::cerr << "Failed to open output file: " << filename_ << std::endl;
        return false;
    }
    return true;
}

bool PCMFrameWriter::writeFrame(const AVFrame* frame) {
    if (!outFile || !frame) return false;

      if (!initialized_) {
        if (!initFromFrame(frame)) return false;
        initialized_ = true;
    }

    return writeImpl_ ? writeImpl_(frame) : false;
}

bool PCMFrameWriter::writePlanar(const AVFrame* frame) {
    for (int i = 0; i < frame->nb_samples; ++i) {
        for (int ch = 0; ch < channels_; ++ch) {
            const uint8_t* ptr = frame->data[ch] + i * bytesPerSample_;
            if (fwrite(ptr, 1, bytesPerSample_, outFile) != (size_t)bytesPerSample_) {
                return false;
            }
        }
    }
    return true;
}

bool PCMFrameWriter::writePacked(const AVFrame* frame) {
    int totalBytes = frame->nb_samples * channels_ * bytesPerSample_;
    return fwrite(frame->data[0], 1, totalBytes, outFile) == (size_t)totalBytes;
}


bool PCMFrameWriter::initFromFrame(const AVFrame* frame) {
    AVSampleFormat fmt = static_cast<AVSampleFormat>(frame->format);
    channels_ = frame->channels;
    bytesPerSample_ = av_get_bytes_per_sample(fmt);
    sampleRate_ = frame->sample_rate; 

    if (channels_ <= 0 || bytesPerSample_ <= 0) {
        std::cerr << "Invalid frame format" << std::endl;
        return false;
    }

    if (av_sample_fmt_is_planar(fmt)) {
        writeImpl_ = [this](const AVFrame* f) { return writePlanar(f); };
    } else {
        writeImpl_ = [this](const AVFrame* f) { return writePacked(f); };
    }
    std::cout << "Audio format info:\n";
    std::cout << "  Sample format  : " << av_get_sample_fmt_name(fmt) << "\n";
    std::cout << "  Channels       : " << channels_ << "\n";
    std::cout << "  Sample rate    : " << sampleRate_ << "\n";
    std::cout << "  Bytes/sample   : " << bytesPerSample_ << "\n";

    return true;
}

void PCMFrameWriter::close() {
    if (outFile) {
        fclose(outFile);
        outFile = nullptr;
    }
}


YUVFrameWriter::YUVFrameWriter(const std::string& filename)
    : filename_(filename),frameindex(0) {}

YUVFrameWriter::~YUVFrameWriter() {
    av_free(video_dst_data[0]);
    close();
}

bool YUVFrameWriter::open() {
    outFile = fopen(filename_.c_str(), "wb");
    if (!outFile) {
        std::cerr << "Failed to open output file: " << filename_ << std::endl;
        return false;
    }
    return true;
}

bool YUVFrameWriter::writeFrame(const AVFrame* frame) {
    if (!frame || !outFile) return false;
        if (video_dst_bufsize == 0) {
        width = frame->width;
        height = frame->height;
        pix_fmt = static_cast<AVPixelFormat>(frame->format);
        int ret = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
        if (ret < 0) {
            std::cerr << "Could not allocate raw video buffer\n";
            return false;
        }
        video_dst_bufsize = ret;
    }
    if (frame->width != width || frame->height != height || frame->format != pix_fmt) {
    std::cerr << "Frame resolution or pixel format changed. Aborting.\n";
    return false;
    }

    av_image_copy(video_dst_data, video_dst_linesize,
                    (const uint8_t **)frame->data, frame->linesize,
                   pix_fmt, width, height);
 
    /* write to rawvideo file */
    fwrite(video_dst_data[0], 1, video_dst_bufsize, outFile);
    return true;
}

void YUVFrameWriter::close() {
    if (outFile) {
        fclose(outFile);
        outFile = nullptr;
    }

}

//分开写帧方式，没有进行校验
// bool YUVFrameWriter::writeFrame(const AVFrame* frame) {
//     char filename[1024];
//     snprintf(filename, sizeof(filename), "%s_%04d.yuv", filename_.c_str(), frameindex);
//     FILE *output_file = fopen(filename, "wb");
//     if(!output_file) {
//         std::cerr << "Could not open output file: " << filename << std::endl;
//         return false;
//     }
//     int width = frame->width;
//     int height = frame->height;

//     for (int y = 0; y < height; y++) {
//         fwrite(frame->data[0] + y * frame->linesize[0], 1, width, output_file);
//     }
//     for (int y = 0; y < height / 2; y++) {
//        fwrite(frame->data[1] + y * frame->linesize[1], 1, width / 2, output_file);
//     }
//     for (int y = 0; y < height / 2; y++) {
//         fwrite(frame->data[2] + y * frame->linesize[2], 1, width / 2, output_file);    
//     }
//     std::cout << "Saved frame " << frameindex++ << " to " << filename << std::endl;
//     fclose(output_file);
//     return true;
// }
