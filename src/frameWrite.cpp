#include "frameWrite.hpp"
#include <iostream>
#include <cassert>

PCMFrameWriter::PCMFrameWriter(const std::string& filename)
    : filename_(filename),channels_(0),sampleRate_(0){}

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

    if (sampleRate_ < 0) {
        std::cerr << "Unsupported sample format" << std::endl;
        return false;
    }
    for (int i = 0; i < frame->nb_samples; i++) {
        for (int ch = 0; ch < channels_; ch++) {
            size_t written = fwrite(frame->data[ch] + i * sampleRate_, 1, sampleRate_, outFile);
            if (written != (size_t)sampleRate_) {
                std::cerr << "Write error" << std::endl;
                return false;
            }
        }
    }
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
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s_%04d.yuv", filename_.c_str(), frameindex);
    FILE *output_file = fopen(filename, "wb");
    if(!output_file) {
        std::cerr << "Could not open output file: " << filename << std::endl;
        return false;
    }
    int width = frame->width;
    int height = frame->height;

    for (int y = 0; y < height; y++) {
        fwrite(frame->data[0] + y * frame->linesize[0], 1, width, output_file);
    }
    for (int y = 0; y < height / 2; y++) {
       fwrite(frame->data[1] + y * frame->linesize[1], 1, width / 2, output_file);
    }
    for (int y = 0; y < height / 2; y++) {
        fwrite(frame->data[2] + y * frame->linesize[2], 1, width / 2, output_file);    
    }
    std::cout << "Saved frame " << frameindex++ << " to " << filename << std::endl;
    fclose(output_file);
    return true;
}

void YUVFrameWriter::close() {
    if (outFile) {
        fclose(outFile);
        outFile = nullptr;
    }
}