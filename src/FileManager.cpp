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

    //检查解码器是否准备
    if (!decoder->isInitialized()) {
        std::cerr << "Decoder is not initialized." << std::endl;
        exit(1);
    }

    //打开输入文件
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
    //创建写入器

    //视频解码
    if(mediaType == MediaType::VIDEO){
        //准备读取缓冲区
        std::string outputFilePath = buildOutputFilePath(outputDir, inputPath, ".yuv");
        std::unique_ptr<FrameWriter> writer = FrameWriterFactory::createWriter(outputFilePath, mediaType);
        writer->open();
        constexpr int bufferSize = INBUF_SIZE;
        uint8_t buffer[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE] = {0};  
        memset(buffer + bufferSize, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        //循环读取
        bool eof = false;
        do  {
            size_t bytesRead = fread(buffer, 1, bufferSize, inputFile);
            if (ferror(inputFile)) {
                std::cerr << "Error reading input file" << std::endl;
                exit(1);
            }
            eof = !bytesRead;
            data = buffer;
            //缓冲区内存在数据时，进行数据解析
            while(bytesRead > 0 || eof) {
                decoder->parsePacket(data, bytesRead,pkt);
                if(pkt->size>0)
                {
                    if(decoder->sendPacketAndReceiveFrame(pkt)){
                        AVFrame* frame = decoder->getFrame();
                        av_packet_unref(pkt);
                        if (!writer->writeFrame(frame)) 
                        {
                            std::cerr << "Failed to save frame "<< std::endl;
                            exit(1);
                        }   
                    }
                } 
                else if (eof)
                    break;
            }      
        }while(!eof);
        decoder->flush();
        AVFrame* frame = decoder->getFrame();
        if (!writer->writeFrame(frame)) 
        {
            std::cerr << "Failed to save frame " << std::endl;
            exit(1);
        } 
        writer->close();  
    }
    //音频解码
    else if (mediaType == MediaType::AUDIO){
        //准备缓冲区
        int len,ret;
        uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
        data = inbuf;
        data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, inputFile);
        std::string outputFilePath = buildOutputFilePath(outputDir, inputPath, ".pcm");
        std::unique_ptr<FrameWriter> writer = FrameWriterFactory::createWriter(outputFilePath, mediaType);
        writer->open();
        //循环读取，
        AVFrame *decoded_frame = NULL;

        while(data_size > 0){
            //数据送入解析器，进行解析。
            decoder->parsePacket(data,data_size,pkt);
            //如果成功解析出一帧数据，送入解码器获得
            if(pkt->size>0){
                 if(decoder->sendPacketAndReceiveFrame(pkt)){
                    AVFrame* frame = decoder->getFrame();
                    av_packet_unref(pkt);
                    int channels = decoder->get_channels();
                    int sampleRats = decoder->get_bytes_per_sample();
                    //成功获得帧后，通过写入器手动持久化
                    if(auto audioWriter = dynamic_cast<AudioExtraInfo*>(writer.get())){
                        audioWriter->setAudioParams(sampleRats,channels);
                    }
                    writer->writeFrame(frame);
                    if(data_size < AUDIO_REFILL_THRESH){
                        memmove(inbuf,data,data_size);
                        data = inbuf;
                        len = fread(data+data_size , 1, AUDIO_INBUF_SIZE - data_size, inputFile);
                        if(len > 0)
                            data_size += len; 
                    }
                 }
            }
        }
        decoder->flush();
        writer->close();
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
    //初始化解复用器

    std::string audioPath = buildOutputFilePath(outputDir,inputPath,".pcm");
    std::string videoPath = buildOutputFilePath(outputDir, inputPath, ".yuv");

    if(demuxer->loadfile(inputPath.c_str())){
        //加载视频流
        demuxer->open_video_format();
        //加载音频流
        demuxer->open_audio_format();
        //根据流媒体信息加载解码器
        decoder_audio->initialize_fromstream(demuxer->get_audiostream()->codecpar);
        decoder_video->initialize_fromstream(demuxer->get_videostream()->codecpar);

        //加载格式参数(视频),并且打开写入器
        std::unique_ptr<FrameWriter> writer_video = FrameWriterFactory::createWriter(videoPath, MediaType::VIDEO);
        std::unique_ptr<FrameWriter> writer_audio = FrameWriterFactory::createWriter(audioPath, MediaType::AUDIO);
        writer_video->open();
        writer_audio->open();

        //循环读取pkt数据并解码
        AVFormatContext* fmt_ctx = demuxer->get_fmx();
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        exit(1);
        }   
        while (av_read_frame(fmt_ctx, pkt) >= 0) {
        //从流中解析出视频帧
        if (pkt->stream_index == demuxer->getVideoStreamIndex()){
            if(ret = decoder_video->sendPacketAndReceiveFrame(pkt)){
                writer_video->writeFrame(decoder_video->getFrame());
            }
        }
        //从流中解析出音频帧
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