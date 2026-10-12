#include "cli_common.h"

#include <keeb/api.h>
#include <keeb/chroma.h>
#include <keeb/dsp.h>
#include <keeb/key.h>
#include <keeb/wav_io.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace keeb;
using namespace keeb::cli;

static void usage() {
    std::fprintf(stderr,
        "Usage:\n"
        "  key_probe [options] <path-to-wav>\n"
        "\n"
        "Options:\n"
        "  --profiles <name>       Key profiles: krumhansl | temperley1999 | temperley2007\n"
        "  --ref-freq <Hz>         A4 reference. Default 440.\n"
        "  --naive-pcp             Use naive PCP instead of HPCP.\n"
        "  --clp                   Use chroma log pitch instead of HPCP.\n"
        "  --chroma-csv <file>     Dump song-level PCP (12-dim) to CSV.\n"
        "  --per-frame-csv <file>  Dump per-frame chroma matrix to CSV.\n"
        "  --help                  Show this help.\n");
}

int main(int argc, char** argv) {
    Params params;
    const char* path = 0;
    const char* chroma_csv = 0;
    const char* per_frame_csv = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else if (std::strcmp(a, "--naive-pcp") == 0) {
            params.use_hpcp = false;
            params.chroma_method = Params::ChromaMethod::NaivePcp;
        } else if (std::strcmp(a, "--clp") == 0) {
            params.chroma_method = Params::ChromaMethod::Clp;
            params.use_hpcp = false;
        } else if (std::strcmp(a, "--profiles") == 0 && i + 1 < argc) {
            if (!parse_profiles(argv[++i], params.key_profiles)) {
                std::fprintf(stderr, "unknown profiles name\n");
                return 64;
            }
        } else if (std::strcmp(a, "--ref-freq") == 0 && i + 1 < argc) {
            params.chroma_ref_freq = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(a, "--chroma-csv") == 0 && i + 1 < argc) {
            chroma_csv = argv[++i];
        } else if (std::strcmp(a, "--per-frame-csv") == 0 && i + 1 < argc) {
            per_frame_csv = argv[++i];
        } else if (a[0] == '-') {
            std::fprintf(stderr, "unknown option: %s\n", a);
            usage();
            return 64;
        } else if (!path) {
            path = a;
        } else {
            return 64;
        }
    }
    if (!path) {
        usage();
        return 64;
    }

    WavData wav;
    const WavError err = load_wav(path, wav);
    if (err != WavError::OK) {
        std::fprintf(stderr, "load_wav: %s\n", wav_error_string(err));
        return wav_error_to_exit(err);
    }

    validate_params(params);
    std::vector<float> samples;
    uint32_t sr = wav.sample_rate;
    if (wav.sample_rate != params.target_sample_rate) {
        downsample_linear(wav.samples.data(),
                          static_cast<uint32_t>(wav.samples.size()),
                          wav.sample_rate,
                          params.target_sample_rate,
                          samples);
        sr = params.target_sample_rate;
    } else {
        samples = wav.samples;
    }

    StftFrames stft = compute_stft(samples.data(),
                                   static_cast<uint32_t>(samples.size()),
                                   sr,
                                   params.fft_size,
                                   params.hop_size,
                                   WindowType::Hann);
    const std::vector<std::vector<float> > chroma =
        compute_chroma(stft.mag, params.fft_size, sr, params);
    const std::vector<float> pcp = aggregate_pcp(chroma);
    KeyResult r = estimate_key(pcp, params);

    if (chroma_csv) {
        write_csv_1d(chroma_csv, "pcp", pcp);
    }
    if (per_frame_csv) {
        write_chroma_csv(per_frame_csv, chroma);
    }

    if (r.key == Key::Unknown) {
        std::fprintf(stderr, "analysis failed (unknown key)\n");
        return 5;
    }

    std::printf("Key: %s (confidence: %.2f)\n", to_string(r.key), static_cast<double>(r.confidence));
    if (!r.alternatives.empty()) {
        std::printf("Alternatives:\n");
        for (size_t i = 0; i < r.alternatives.size(); ++i) {
            std::printf("  %zu. %-12s %.2f\n",
                        i + 1,
                        to_string(r.alternatives[i].key),
                        static_cast<double>(r.alternatives[i].confidence));
        }
    }
    return 0;
}
