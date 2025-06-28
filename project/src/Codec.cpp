#include "Codec.hpp"
#include "zlib.h"
#define BUFFER_SIZE 32768


bool Codec::compress_data(std::string& compressed_data, const void* data, const size_t data_size, const bool gzip)
{
    z_stream zs = {0};

    // Set windowBits: 15 for deflate (zlib wrapper). Add 16 to enable gzip wrapper.
    int windowBits = 15;
    if (gzip) {
        windowBits += 16;
    }

    // Initialize the deflate stream.
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     windowBits, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        return false;
    }

    // Set input data: cast away const to satisfy deflate's API.
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(data));
    zs.avail_in = static_cast<uInt>(data_size);

    int ret;
    char buffer[BUFFER_SIZE];

    // Compress the data in a loop.
    do 
    {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);

        ret = deflate(&zs, Z_FINISH);
        if (compressed_data.size() < zs.total_out) 
        {
            // Append the newly compressed data.
            compressed_data.append(buffer, zs.total_out - compressed_data.size());
        }
    } while (ret == Z_OK);

    // Clean up.
    deflateEnd(&zs);

    // Check if compression ended successfully.
    if (ret != Z_STREAM_END) {
        return false;
    }

    return true;
}

bool Codec::decompress_data(std::string& decompressed_data, const void* data, const size_t data_size, const bool gzip)
{
    z_stream zs = {0};

    // windowBits: 15 for zlib format; add 16 if using gzip format.
    int windowBits = 15;
    if (gzip) {
        windowBits += 16;
    }

    if (inflateInit2(&zs, windowBits) != Z_OK) {
        return false;
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(data));
    zs.avail_in = static_cast<uInt>(data_size);

    int ret;
    char buffer[BUFFER_SIZE];

    // Decompress until the end of stream.
    do 
    {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);

        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            return false;
        }

        // Calculate number of bytes decompressed in this iteration.
        size_t bytesDecompressed = sizeof(buffer) - zs.avail_out;
        decompressed_data.append(buffer, bytesDecompressed);
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return true;
}


bool Codec::compress_string(std::string& compressed_data, const std::string& data, const bool gzip) 
{
    return compress_data(compressed_data, data.data(), data.size(), gzip);
}


bool Codec::decompress_string(std::string& decompressed_data, const std::string& data, const bool gzip)
{
    return decompress_data(decompressed_data, data.data(), data.size(), gzip);
}

