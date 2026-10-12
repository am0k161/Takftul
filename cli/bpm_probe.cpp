#include "cli_common.h"

#include <taktful/api.h>
#include <taktful/dsp.h>
#include <taktful/onset.h>
#include <taktful/tempo.h>
#include <taktful/wav_io.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace taktful;
using namespace taktful::cli;

static void usage() {
    std::fprintf(stderr,
        "Usage:\n"
        "  bpm_probe [options] <path-to-wav>\n"
        "\n"
        "Options:\n"
        "  --min <bpm>             Minimum tempo. Default 60.\n"
        "  --max <bpm>             Maximum tempo. Default 200.\n"
        "  --no-dp                 Disable DP refinement.\n"
        "  --text                  Output text (default).\n"
        "  --json                  Output JSON.\n"
        "  --onset-csv <file>      Dump onset envelope to CSV for plotting.\n"
        "  --acorr-csv <file>      Dump autocorrelation curve to CSV for plotting.\n"
        "  --help                  Show this help.\n");
}

int main(int argc, char** argv) {
    Params params;
    params.enable_dp_refinement = true;
    bool as_json = false;
    const char* path = 0;
    const char* onset_csv = 0;
    const char* acorr_csv = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else if (std::strcmp(a, "--text") == 0) {
            as_json = false;
        } else if (std::strcmp(a, "--json") == 0) {
            as_json = true;
        } else if (std::strcmp(a, "--no-dp") == 0) {
            params.enable_dp_refinement = false;
        } else if (std::strcmp(a, "--min") == 0 && i + 1 < argc) {
            params.tempo_min_bpm = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(a, "--max") == 0 && i + 1 < argc) {
            params.tempo_max_bpm = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(a, "--onset-csv") == 0 && i + 1 < argc) {
            onset_csv = argv[++i];
        } else if (std::strcmp(a, "--acorr-csv") == 0 && i + 1 < argc) {
            acorr_csv = argv[++i];
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
    const bool hfc = (params.onset_method == Params::OnsetMethod::SpectralFluxHFC);
    const std::vector<float> onset = compute_onset_envelope(stft.mag, hfc);
    const float hop_sec = static_cast<float>(params.hop_size) / static_cast<float>(sr);
    TempoDebug dbg = estimate_tempo_debug(onset, hop_sec, params);
    TempoResult r = dbg.result;

    if (onset_csv) {
        write_csv_1d(onset_csv, "onset", onset);
    }
    if (acorr_csv) {
        write_csv_1d(acorr_csv, "acorr", dbg.acorr);
    }

    if (r.bpm <= 0.0f) {
        std::fprintf(stderr, "analysis failed (no tempo)\n");
        return 5;
    }

    const char* method = params.enable_dp_refinement ? "autocorrelation + DP" : "autocorrelation";
    if (as_json) {
        std::printf("{\"bpm\": %.1f, \"confidence\": %.2f, \"method\": \"%s\"}\n",
                    static_cast<double>(r.bpm),
                    static_cast<double>(r.confidence),
                    method);
    } else {
        std::printf("BPM: %.1f (confidence: %.2f, method: %s)\n",
                    static_cast<double>(r.bpm),
                    static_cast<double>(r.confidence),
                    method);
    }
    return 0;
}
