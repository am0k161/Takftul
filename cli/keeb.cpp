#include "cli_common.h"

#include <keeb/api.h>
#include <keeb/wav_io.h>

#include <cstdio>
#include <cstring>
#include <string>

using namespace keeb;
using namespace keeb::cli;

static void usage() {
    std::fprintf(stderr,
        "keeb — analyze a WAV file for key and BPM\n"
        "\n"
        "Usage:\n"
        "  keeb [path-to-wav]\n"
        "  keeb\n"
        "\n"
        "With no argument, prompts for a file path.\n");
}

static std::string read_line() {
    char buf[1024];
    if (!std::fgets(buf, sizeof(buf), stdin)) {
        return std::string();
    }
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    if (s.size() >= 2 && s[i] == '"' && s.back() == '"') {
        return s.substr(i + 1, s.size() - i - 2);
    }
    return s.substr(i);
}

static int analyze_path(const char* path) {
    WavData wav;
    const WavError err = load_wav(path, wav);
    if (err != WavError::OK) {
        std::fprintf(stderr, "error: %s\n", wav_error_string(err));
        return wav_error_to_exit(err);
    }

    const double duration = (wav.sample_rate > 0)
        ? static_cast<double>(wav.samples.size()) / static_cast<double>(wav.sample_rate)
        : 0.0;

    Params params;
    AnalysisResult r = analyze(wav, params);
    if (r.tempo.bpm <= 0.0f && r.key.key == Key::Unknown) {
        std::printf("file:     %s\n", path);
        std::printf("duration: %.2f s\n", duration);
        std::printf("result:   no key/tempo (silent or too short)\n");
        return 5;
    }

    std::printf("file:     %s\n", path);
    std::printf("duration: %.2f s\n", duration);
    std::printf("tempo:    %.1f BPM  (confidence %.2f)\n",
                static_cast<double>(r.tempo.bpm),
                static_cast<double>(r.tempo.confidence));
    std::printf("key:      %s  [%s]  (confidence %.2f)\n",
                to_string(r.key.key),
                to_camelot(r.key.key),
                static_cast<double>(r.key.confidence));
    if (!r.key.alternatives.empty()) {
        std::printf("alts:     ");
        for (size_t i = 0; i < r.key.alternatives.size(); ++i) {
            if (i) std::printf(", ");
            std::printf("%s %.2f",
                        to_string(r.key.alternatives[i].key),
                        static_cast<double>(r.key.alternatives[i].confidence));
        }
        std::printf("\n");
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* path = 0;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else if (a[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", a);
            usage();
            return 64;
        } else if (!path) {
            path = a;
        } else {
            usage();
            return 64;
        }
    }

    std::string held;
    if (!path) {
        std::printf("WAV path: ");
        std::fflush(stdout);
        held = read_line();
        if (held.empty()) {
            std::fprintf(stderr, "no path given\n");
            return 64;
        }
        path = held.c_str();
    }
    return analyze_path(path);
}
