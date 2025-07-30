#include <string>
#include "baseDecoder.hpp"
#include "frameWrite.hpp"
#include "demuxer.hpp"



class FileManager {
public:
    FileManager();
    ~FileManager();
    void process_raw(const std::string& inputPath,
                     const std::string& outputPath,
                     MediaType mediaType,
                     BaseDecoder* decoder
                    );

    void process_mux(const std::string& inputPath,
                     const std::string& outputPath,
                     Demuxer* demuxer,
                     BaseDecoder* decoder_audio,
                     BaseDecoder* decoder_video);

private:
    std::string buildOutputFilePath(const std::string& outputDir, const std::string& inputPath, const std::string& newSuffix);
};
