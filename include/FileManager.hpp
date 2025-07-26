#include <string>
#include "baseDecoder.hpp"
#include "frameWrite.hpp"



class FileManager {
public:
    FileManager(const std::string& inputPath, const std::string& outputPath,MediaType type,BaseDecoder* decoder);
    ~FileManager();
    void process();

private:
    std::string inputFilePath;
    std::string outputFilePath;
    FILE* inputFile;

    BaseDecoder* decoder;
    std::unique_ptr<FrameWriter>writer;

    const int bufferSize = INBUF_SIZE;
    MediaType mediaType;
    bool writerInitialized = false;
    void initializeWriter(const AVFrame* frame);
};
