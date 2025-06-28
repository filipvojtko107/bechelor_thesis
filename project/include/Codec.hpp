#ifndef __CODEC_HPP__
#define __CODEC_HPP__
#include <string>

namespace Codec
{
    bool compress_data(std::string& compressed_data, const void* data, const size_t dataSize, const bool gzip);
    bool decompress_data(std::string& decompressed_data, const void* data, const size_t data_size, const bool gzip);
    bool compress_string(std::string& compressed_data, const std::string& data, const bool gzip);
    bool decompress_string(std::string& decompressed_data, const std::string& data, const bool gzip);
}

#endif