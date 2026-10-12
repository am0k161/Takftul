#ifndef KEEB_WAV_IO_H
#define KEEB_WAV_IO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace keeb {

struct WavData {
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    std::vector<float> samples;
};

enum class WavError {
    OK = 0,
    FileNotFound,
    NotAWav,
    UnsupportedFormat,
    UnsupportedBitDepth,
    OutOfMemory,
    Truncated,
};

WavError load_wav(const char* path, WavData& out);
WavError load_wav_mem(const void* data, std::size_t nbytes, WavData& out);

} // namespace keeb

#endif
