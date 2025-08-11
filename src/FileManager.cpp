#include "FileManager.hpp"
#include <iostream>
#include <cstdio>
#include <filesystem>

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
FileManager::FileManager(){}

FileManager::~FileManager() {}

void FileManager::process_raw(const std::string& inputPath,
                     const std::string& outputDir,
                     MediaType mediaType,
                     BaseDecoder* decoder){
    
    uint8_t *data;
    size_t   data_size;

    if (!decoder->isInitialized()) {
        std::cerr << "Decoder is not initialized." << std::endl;
        exit(1);
    }

    FILE* inputFile = fopen(inputPath.c_str(), "rb");
    if (!inputFile) {
        std::cerr << "Failed to open input file: " << inputPath << std::endl;
        exit(1);
    }

    AVPacket* pkt = av_packet_alloc();
     if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        exit(1);
        }   
    switch(mediaType){
        case MediaType::VIDEO:
            processVideo(inputFile,inputPath,outputDir,decoder,pkt,data,data_size);
            break;
        case MediaType::AUDIO:
            processAudio(inputFile,inputPath,outputDir,decoder,pkt,data,data_size);
            break;
    }
    fclose(inputFile);
    av_packet_free(&pkt);
}

void FileManager::process_mux(const std::string& inputPath,
                     const std::string& outputDir,
                     Demuxer* demuxer,
                     BaseDecoder* decoder_audio,
                     BaseDecoder* decoder_video){
    int ret = 0;

    std::string audioPath = buildOutputFilePath(outputDir,inputPath,".pcm");
    std::string videoPath = buildOutputFilePath(outputDir, inputPath, ".yuv");

    if(demuxer->loadfile(inputPath.c_str())){
        demuxer->open_video_format();
        demuxer->open_audio_format();
        decoder_audio->initialize_fromstream(demuxer->get_audiostream()->codecpar);
        decoder_video->initialize_fromstream(demuxer->get_videostream()->codecpar);
        std::unique_ptr<FrameWriter> writer_video = FrameWriterFactory::createWriter(videoPath, MediaType::VIDEO);
        std::unique_ptr<FrameWriter> writer_audio = FrameWriterFactory::createWriter(audioPath, MediaType::AUDIO);
        writer_video->open();
        writer_audio->open();
        AVFormatContext* fmt_ctx = demuxer->get_fmx();
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        exit(1);
        }   
        while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == demuxer->getVideoStreamIndex()){
            if(ret = decoder_video->sendPacketAndReceiveFrame(pkt)){
                writer_video->writeFrame(decoder_video->getFrame());
            }
        }
        else if (pkt->stream_index == demuxer->getAudioStreamIndex()){
            if(ret = decoder_audio->sendPacketAndReceiveFrame(pkt)){
                writer_audio->writeFrame(decoder_audio->getFrame());
            }
        }
        av_packet_unref(pkt);
        if (ret < 0)
            break;
    }
    decoder_audio->flush();
    writer_audio->writeFrame(decoder_audio->getFrame());

    decoder_video->flush();
    writer_video->writeFrame(decoder_video->getFrame()); 
    av_packet_free(&pkt);
    }
}

std::string FileManager::buildOutputFilePath(const std::string& outputDir, const std::string& inputPath, const std::string& newSuffix) {
    namespace fs = std::filesystem;

    fs::path input(inputPath);
    fs::path dir(outputDir);
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::string baseName = input.stem().string();
    fs::path outputFile = dir / (baseName + newSuffix);

    return outputFile.string();
}

void FileManager::processVideo(FILE* inputFile, const std::string& inputPath,
                               const std::string& outputDir, BaseDecoder* decoder,
                               AVPacket* pkt, uint8_t*& data, size_t& data_size) {
    std::string outputFilePath = buildOutputFilePath(outputDir, inputPath, ".yuv");
    auto writer = FrameWriterFactory::createWriter(outputFilePath, MediaType::VIDEO);
    writer->open();

    constexpr int bufferSize = INBUF_SIZE;
    uint8_t buffer[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {0};  
    memset(buffer + bufferSize, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    bool isFirstFrame = true; 
    bool eof = false;
    do {
        size_t bytesRead = fread(buffer, 1, bufferSize, inputFile);
        if (ferror(inputFile)) {
            std::cerr << "Error reading input file" << std::endl;
            exit(1);
        }
        eof = !bytesRead;
        data = buffer;
        
        while (bytesRead > 0 || eof) {
            decoder->parsePacket(data, bytesRead, pkt);
            if (pkt->size > 0 && decoder->sendPacketAndReceiveFrame(pkt)) {
                AVFrame* frame = decoder->getFrame();
                if(isFirstFrame){
                    savedWidth = frame->width;
                    savedHeight = frame->height;
                    pix_fmt = (AVPixelFormat)frame->format;
                    videoTimebase = {1, 30};
                    sample_aspect_ratio = frame->sample_aspect_ratio;
                    videoFilter = std::make_shared<VideoFilter>(currentParamsVideo,
                                                    savedWidth,
                                                    savedHeight,
                                                    pix_fmt,
                                                    videoTimebase,
                                                    sample_aspect_ratio);
                    isFirstFrame = false;       
                }

                av_packet_unref(pkt);
                if(enableVideoFilter){
                    videoFilter->push_frame(frame);
                     while (AVFrame* filt = videoFilter->pull_frame()) {
                        writer->writeFrame(filt);
                        av_frame_free(&filt);
                    }
                }
                else{
                    if (!writer->writeFrame(frame)) {
                    std::cerr << "Failed to save frame\n";
                    exit(1);
                    }
                }
            } else if (eof) {
                break;
            }
        }
    } while (!eof);

    decoder->flush();
    if (AVFrame* frame = decoder->getFrame()) {
        writer->writeFrame(frame);
    }
    writer->close();
}

void FileManager::processAudio(FILE* inputFile, const std::string& inputPath,
                               const std::string& outputDir, BaseDecoder* decoder,
                               AVPacket* pkt, uint8_t*& data, size_t& data_size) {
        int len,ret;
        uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
        data = inbuf;
        data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, inputFile);
        std::string outputFilePath = buildOutputFilePath(outputDir, inputPath, ".pcm");
        std::unique_ptr<FrameWriter> writer = FrameWriterFactory::createWriter(outputFilePath, MediaType::AUDIO);
        writer->open();
        //循环读取，
        AVFrame *decoded_frame = NULL;
        bool isFirstFrame = true; 
        
        while(data_size > 0){
            decoder->parsePacket(data,data_size,pkt);
            if(pkt->size>0 && decoder->sendPacketAndReceiveFrame(pkt)){
                    AVFrame* frame = decoder->getFrame();
                    av_packet_unref(pkt);
                    int channels = decoder->get_channels();
                    int sampleRats = decoder->get_bytes_per_sample();
                    if(isFirstFrame){
                        savedSampleFmt = (AVSampleFormat)frame->format;
                        savedChannelLayout = frame->channel_layout ? frame->channel_layout : av_get_default_channel_layout(frame->channels);
                        savedSampleRate = frame->sample_rate;
                        savedTimeBase = {1, savedSampleRate}; 

                        audioFilter = std::make_shared<AudioFilter>(currentParamsAudio,
                                                    savedSampleFmt,
                                                    savedChannelLayout,
                                                    savedSampleRate,
                                                    savedTimeBase);
                        isFirstFrame = false;       
                    }

                    if (enableAudioFilter){
                        audioFilter->push_frame(frame);
                        while (AVFrame* filt = audioFilter->pull_frame()) {
                            if (auto audioWriter = dynamic_cast<AudioExtraInfo*>(writer.get())) {
                                audioWriter->setAudioParams(av_get_bytes_per_sample((AVSampleFormat)filt->format),
                                                            filt->channels);
                            }
                            writer->writeFrame(filt);
                            av_frame_free(&filt);
                        }
                    }
                    else{
                        if(auto audioWriter = dynamic_cast<AudioExtraInfo*>(writer.get())){
                            audioWriter->setAudioParams(sampleRats,channels);
                    }
                    writer->writeFrame(frame);
                    }
                   
                    if(data_size < AUDIO_REFILL_THRESH){
                        memmove(inbuf,data,data_size);
                        data = inbuf;
                        len = fread(data+data_size , 1, AUDIO_INBUF_SIZE - data_size, inputFile);
                        if(len > 0)
                            data_size += len; 
                    }
            }
        }
        decoder->flush();
        writer->close();
        if (enableAudioFilter) {
            closeAudioFilter();
        }
    }
void FileManager::updateFilterAudioParams(const AudioFilterParamUpdate& update) {
    if (update.volume.has_value())
        currentParamsAudio.volume = update.volume.value();
    if (update.tempo.has_value())
        currentParamsAudio.tempo = update.tempo.value();
    if (update.enable_echo.has_value())
        currentParamsAudio.enable_echo = update.enable_echo.value();
    if (update.echo_in_delay.has_value())
        currentParamsAudio.echo_in_delay = update.echo_in_delay.value();
    if (update.echo_in_decay.has_value())
        currentParamsAudio.echo_in_decay = update.echo_in_decay.value();
    if (update.echo_out_delay.has_value())
        currentParamsAudio.echo_out_delay = update.echo_out_delay.value();
    if (update.echo_out_decay.has_value())
        currentParamsAudio.echo_out_decay = update.echo_out_decay.value();
    if (update.enable_lowpass.has_value())
        currentParamsAudio.enable_lowpass = update.enable_lowpass.value();
    if (update.lowpass_freq.has_value())
        currentParamsAudio.lowpass_freq = update.lowpass_freq.value();
    if (update.enable_highpass.has_value())
        currentParamsAudio.enable_highpass = update.enable_highpass.value();
    if (update.highpass_freq.has_value())
        currentParamsAudio.highpass_freq = update.highpass_freq.value();
}

void FileManager::openAudioFilter(){
    enableAudioFilter = true;
}

void FileManager::closeAudioFilter() {
    if (audioFilter) {
        audioFilter.reset();
    }
}


void FileManager::updateFilterVideoParams(const VideoFilterParamsUpdate& update) {

    std::optional<float> brightness;
    std::optional<float> contrast;
    std::optional<float> saturation;
    std::optional<int> rotate;
    std::optional<float> hue;
    std::optional<bool> enable_grayscale;
    std::optional<bool> enable_sepia;
    std::optional<int> blur_radius;

    if (update.brightness.has_value())
        currentParamsVideo.brightness = update.brightness.value();
    if (update.contrast.has_value())
        currentParamsVideo.contrast = update.contrast.value();
    if (update.saturation.has_value())
        currentParamsVideo.saturation = update.saturation.value();
    if (update.rotate.has_value())
        currentParamsVideo.rotate = update.rotate.value();
    if (update.hue.has_value())
        currentParamsVideo.hue = update.hue.value();
    if (update.enable_grayscale.has_value())
        currentParamsVideo.enable_grayscale = update.enable_grayscale.value();
    if (update.enable_sepia.has_value())
        currentParamsVideo.enable_sepia = update.enable_sepia.value();
    if (update.blur_radius.has_value())
        currentParamsVideo.blur_radius = update.blur_radius.value();
}

void FileManager::openVideoFilter(){
    enableVideoFilter = true;
}

void FileManager::closeVideoFilter() {
    if (videoFilter) {
        videoFilter.reset();
    }
}
