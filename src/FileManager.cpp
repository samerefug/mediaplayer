#include "FileManager.hpp"
#include <iostream>
#include <cstdio>

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
FileManager::FileManager(const std::string& inputPath, const std::string& outputPath, MediaType type,BaseDecoder* decoder)
    : inputFilePath(inputPath), outputFilePath(outputPath),mediaType(type),decoder(decoder)
{}

FileManager::~FileManager() {
    if (inputFile) {
        fclose(inputFile);
    }
    decoder = nullptr;
}

void FileManager::process(){
    
    uint8_t *data;
    size_t   data_size;

    //检查解码器是否准备
    if (!decoder->isInitialized()) {
        std::cerr << "Decoder is not initialized." << std::endl;
        exit(1);
    }

    //打开输入文件
    inputFile = fopen(inputFilePath.c_str(), "rb");
    if (!inputFile) {
        std::cerr << "Failed to open input file: " << inputFilePath << std::endl;
        exit(1);
    }

    //创建写入器
     std::unique_ptr<FrameWriter> writer = FrameWriterFactory::createWriter(outputFilePath, mediaType);
     writer->open();
    //视频解码
    if(mediaType == MediaType::VIDEO){
        //准备读取缓冲区
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
                decoder->loadPacket(data, bytesRead);
                if(decoder->sendPacketAndReceiveFrame())
                {
                    AVFrame* frame = decoder->getFrame();
                    if (!writer->writeFrame(frame)) 
                    {
                        std::cerr << "Failed to save frame "<< std::endl;
                        exit(1);
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
    
    }
    //音频解码
    else if (mediaType == MediaType::AUDIO){
        //准备缓冲区
        int len,ret;
        uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
        data = inbuf;
        data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, inputFile);
        //循环读取，
        AVFrame *decoded_frame = NULL;

        while(data_size > 0){
            //数据送入解析器，进行解析。
            decoder->loadPacket(data,data_size);
            //如果成功解析出一帧数据，送入解码器获得
            if(decoder->sendPacketAndReceiveFrame()){
                 AVFrame* frame = decoder->getFrame();
                 int channels = decoder->get_channels();
                 int sampleRats = decoder->get_bytes_per_sample();
                 //成功获得帧后，通过写入器手动持久化
                if(auto audioWriter = dynamic_cast<AudioExtraInfo*>(writer.get())){
                    audioWriter->setAudioParams(sampleRats,channels);
                }
                writer->writeFrame(frame);

                 //帧数据获得后，手动填充缓冲区数据,并且继续从文件中读取数据进入缓存
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
    }
    writer->close();
}

void FileManager::initializeWriter(const AVFrame* frame){
    if(writerInitialized){
        writer = FrameWriterFactory::createWriter(outputFilePath,mediaType);
        if(!writer || !writer->open()){
            std::cerr <<"failed to open framewrite"<<std::endl;
            exit(1);
        }
        writerInitialized = true;
    }
}