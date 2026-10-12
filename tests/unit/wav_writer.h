#ifndef TAKTFUL_TEST_WAV_WRITER_H
#define TAKTFUL_TEST_WAV_WRITER_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

namespace taktful {
namespace test {

inline void wr_u16(std::vector<unsigned char>& b, uint16_t v) {
    b.push_back(static_cast<unsigned char>(v & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}
inline void wr_u32(std::vector<unsigned char>& b, uint32_t v) {
    b.push_back(static_cast<unsigned char>(v & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 24) & 0xff));
}
inline void wr_u64(std::vector<unsigned char>& b, uint64_t v) {
    wr_u32(b, static_cast<uint32_t>(v & 0xffffffffu));
    wr_u32(b, static_cast<uint32_t>(v >> 32));
}

inline bool write_pcm16_wav(const char* path,
                            const std::vector<float>& mono,
                            uint32_t sample_rate,
                            uint16_t channels = 1) {
    std::vector<unsigned char> data;
    const uint32_t n = static_cast<uint32_t>(mono.size());
    data.reserve(n * channels * 2);
    for (uint32_t i = 0; i < n; ++i) {
        float s = mono[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        const int16_t v = static_cast<int16_t>(s * 32767.0f);
        for (uint16_t c = 0; c < channels; ++c) {
            wr_u16(data, static_cast<uint16_t>(v));
        }
    }
    std::vector<unsigned char> out;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    wr_u32(out, 36 + static_cast<uint32_t>(data.size()));
    out.insert(out.end(), {'W', 'A', 'V', 'E'});
    out.insert(out.end(), {'f', 'm', 't', ' '});
    wr_u32(out, 16);
    wr_u16(out, 1);
    wr_u16(out, channels);
    wr_u32(out, sample_rate);
    wr_u32(out, sample_rate * channels * 2);
    wr_u16(out, static_cast<uint16_t>(channels * 2));
    wr_u16(out, 16);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    wr_u32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());

    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t nwrite = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return nwrite == out.size();
}

inline bool write_pcm_wav_bits(const char* path,
                               const std::vector<float>& mono,
                               uint32_t sample_rate,
                               uint16_t bits,
                               uint16_t channels = 1,
                               bool as_float = false) {
    std::vector<unsigned char> data;
    const uint32_t n = static_cast<uint32_t>(mono.size());
    const uint16_t bps = as_float ? 32 : bits;
    const uint16_t fmt = as_float ? 3 : 1;
    for (uint32_t i = 0; i < n; ++i) {
        float s = mono[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        for (uint16_t c = 0; c < channels; ++c) {
            if (as_float) {
                unsigned char tmp[4];
                std::memcpy(tmp, &s, 4);
                data.insert(data.end(), tmp, tmp + 4);
            } else if (bps == 8) {
                data.push_back(static_cast<unsigned char>(s * 127.0f + 128.0f));
            } else if (bps == 16) {
                wr_u16(data, static_cast<uint16_t>(static_cast<int16_t>(s * 32767.0f)));
            } else if (bps == 24) {
                const int32_t v = static_cast<int32_t>(s * 8388607.0f);
                data.push_back(static_cast<unsigned char>(v & 0xff));
                data.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
                data.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
            } else {
                wr_u32(data, static_cast<uint32_t>(static_cast<int32_t>(s * 2147483647.0f)));
            }
        }
    }
    std::vector<unsigned char> out;
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    wr_u32(out, 36 + static_cast<uint32_t>(data.size()));
    out.insert(out.end(), {'W', 'A', 'V', 'E'});
    out.insert(out.end(), {'f', 'm', 't', ' '});
    wr_u32(out, 16);
    wr_u16(out, fmt);
    wr_u16(out, channels);
    wr_u32(out, sample_rate);
    wr_u32(out, sample_rate * channels * (bps / 8));
    wr_u16(out, static_cast<uint16_t>(channels * (bps / 8)));
    wr_u16(out, bps);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    wr_u32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t nwrite = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return nwrite == out.size();
}

inline std::vector<float> sine(float freq, float sr, float seconds, float amp = 0.5f) {
    const uint32_t n = static_cast<uint32_t>(sr * seconds);
    std::vector<float> x(n);
    const float w = 2.0f * 3.14159265358979323846f * freq / sr;
    for (uint32_t i = 0; i < n; ++i) {
        x[i] = amp * std::sin(w * static_cast<float>(i));
    }
    return x;
}

inline std::vector<float> click_track(float bpm, float sr, float seconds, float amp = 0.9f) {
    const uint32_t n = static_cast<uint32_t>(sr * seconds);
    std::vector<float> x(n, 0.0f);
    const float period = 60.0f / bpm;
    const uint32_t click_len = static_cast<uint32_t>(0.008f * sr);
    for (float t = 0.0f; t < seconds; t += period) {
        const uint32_t start = static_cast<uint32_t>(t * sr);
        for (uint32_t i = 0; i < click_len && start + i < n; ++i) {
            const float env = 1.0f - static_cast<float>(i) / static_cast<float>(click_len);
            x[start + i] = amp * env;
        }
    }
    return x;
}

inline std::vector<float> triad(float f1, float f2, float f3, float sr, float seconds, float amp = 0.3f) {
    std::vector<float> a = sine(f1, sr, seconds, amp);
    std::vector<float> b = sine(f2, sr, seconds, amp);
    std::vector<float> c = sine(f3, sr, seconds, amp);
    for (size_t i = 0; i < a.size(); ++i) {
        a[i] += b[i] + c[i];
    }
    return a;
}

inline bool write_rf64_pcm16(const char* path,
                             const std::vector<float>& mono,
                             uint32_t sample_rate) {
    std::vector<unsigned char> data;
    const uint32_t n = static_cast<uint32_t>(mono.size());
    data.reserve(n * 2);
    for (uint32_t i = 0; i < n; ++i) {
        float s = mono[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        wr_u16(data, static_cast<uint16_t>(static_cast<int16_t>(s * 32767.0f)));
    }
    const uint64_t data_bytes = data.size();
    std::vector<unsigned char> out;
    out.insert(out.end(), {'R', 'F', '6', '4'});
    wr_u32(out, 0xFFFFFFFFu);
    out.insert(out.end(), {'W', 'A', 'V', 'E'});
    out.insert(out.end(), {'d', 's', '6', '4'});
    wr_u32(out, 28);
    wr_u64(out, 4ull + 8ull + 28ull + 8ull + 16ull + 8ull + data_bytes);
    wr_u64(out, data_bytes);
    wr_u64(out, static_cast<uint64_t>(n));
    wr_u32(out, 0);
    out.insert(out.end(), {'f', 'm', 't', ' '});
    wr_u32(out, 16);
    wr_u16(out, 1);
    wr_u16(out, 1);
    wr_u32(out, sample_rate);
    wr_u32(out, sample_rate * 2);
    wr_u16(out, 2);
    wr_u16(out, 16);
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    wr_u32(out, 0xFFFFFFFFu);
    out.insert(out.end(), data.begin(), data.end());
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    const size_t nwrite = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return nwrite == out.size();
}

inline std::string tmp_path(const char* name) {
    return std::string("/tmp/") + name;
}

} // namespace test
} // namespace taktful

#endif
