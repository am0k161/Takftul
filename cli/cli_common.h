#ifndef KEEB_CLI_COMMON_H
#define KEEB_CLI_COMMON_H

#include <keeb/api.h>
#include <keeb/wav_io.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace keeb {
namespace cli {

inline int wav_error_to_exit(WavError e) {
    switch (e) {
    case WavError::OK: return 0;
    case WavError::FileNotFound: return 1;
    case WavError::NotAWav: return 2;
    case WavError::Truncated: return 2;
    case WavError::UnsupportedFormat: return 3;
    case WavError::UnsupportedBitDepth: return 3;
    case WavError::OutOfMemory: return 4;
    }
    return 2;
}

inline const char* wav_error_string(WavError e) {
    switch (e) {
    case WavError::OK: return "ok";
    case WavError::FileNotFound: return "file not found";
    case WavError::NotAWav: return "not a WAV file";
    case WavError::UnsupportedFormat: return "unsupported WAV format";
    case WavError::UnsupportedBitDepth: return "unsupported bit depth";
    case WavError::OutOfMemory: return "out of memory";
    case WavError::Truncated: return "truncated WAV file";
    }
    return "unknown error";
}

inline bool parse_profiles(const char* s, Params::KeyProfiles& out) {
    if (!s) return false;
    if (std::strcmp(s, "krumhansl") == 0) {
        out = Params::KeyProfiles::KrumhanslOriginal;
        return true;
    }
    if (std::strcmp(s, "temperley1999") == 0) {
        out = Params::KeyProfiles::Temperley1999;
        return true;
    }
    if (std::strcmp(s, "temperley2007") == 0) {
        out = Params::KeyProfiles::Temperley2007;
        return true;
    }
    return false;
}

inline bool write_csv_1d(const char* path, const char* header, const std::vector<float>& v) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "frame_index,%s\n", header);
    for (size_t i = 0; i < v.size(); ++i) {
        std::fprintf(f, "%zu,%.6f\n", i, static_cast<double>(v[i]));
    }
    std::fclose(f);
    return true;
}

inline bool write_chroma_csv(const char* path, const std::vector<std::vector<float> >& chroma) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f, "frame_index,C,C#,D,D#,E,F,F#,G,G#,A,A#,B\n");
    for (size_t t = 0; t < chroma.size(); ++t) {
        std::fprintf(f, "%zu", t);
        for (int p = 0; p < 12; ++p) {
            const float v = (p < static_cast<int>(chroma[t].size())) ? chroma[t][static_cast<size_t>(p)] : 0.0f;
            std::fprintf(f, ",%.6f", static_cast<double>(v));
        }
        std::fprintf(f, "\n");
    }
    std::fclose(f);
    return true;
}

inline std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\\' || c == '"') {
            o.push_back('\\');
        }
        o.push_back(c);
    }
    return o;
}

} // namespace cli
} // namespace keeb

#endif
