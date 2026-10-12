#include "cli_common.h"
#include "../tests/unit/wav_writer.h"

#include <keeb/api.h>
#include <keeb/wav_io.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

using namespace keeb;
using namespace keeb::cli;
using namespace keeb::test;

struct Case {
    std::string path;
    float bpm;
    bool has_bpm;
    Key key;
    bool has_key;
};

static Key parse_key_name(const char* s) {
    if (!s) return Key::Unknown;
    for (int i = 0; i <= 24; ++i) {
        const Key k = static_cast<Key>(i);
        if (std::strcmp(to_string(k), s) == 0) {
            return k;
        }
    }
    return Key::Unknown;
}

static bool relative_ok(Key a, Key b) {
    const int ia = static_cast<int>(a);
    const int ib = static_cast<int>(b);
    if (ia > 23 || ib > 23) return false;
    if (ia == ib) return true;
    if (ia < 12 && ib >= 12) return ((ia + 9) % 12) == (ib % 12);
    if (ia >= 12 && ib < 12) return ((ib + 9) % 12) == (ia % 12);
    return false;
}

static bool load_expected_json(const std::string& path, Case& c) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    std::string txt;
    char buf[512];
    while (std::fgets(buf, sizeof(buf), f)) {
        txt += buf;
    }
    std::fclose(f);
    const char* bpm_tag = std::strstr(txt.c_str(), "\"bpm\"");
    if (bpm_tag) {
        const char* p = std::strchr(bpm_tag, ':');
        if (p) {
            c.bpm = static_cast<float>(std::atof(p + 1));
            c.has_bpm = true;
        }
    }
    const char* key_tag = std::strstr(txt.c_str(), "\"key\"");
    if (key_tag) {
        const char* q = std::strchr(key_tag, ':');
        if (q) {
            const char* a = std::strchr(q, '"');
            if (a) {
                const char* b = std::strchr(a + 1, '"');
                if (b) {
                    std::string name(a + 1, b);
                    c.key = parse_key_name(name.c_str());
                    c.has_key = c.key != Key::Unknown;
                }
            }
        }
    }
    return c.has_bpm || c.has_key;
}

static bool ends_with(const std::string& s, const char* suf) {
    const size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

static std::vector<Case> scan_dir(const char* dir) {
    std::vector<Case> out;
    DIR* d = opendir(dir);
    if (!d) return out;
    struct dirent* e;
    while ((e = readdir(d)) != 0) {
        std::string name = e->d_name;
        if (!ends_with(name, ".wav") && !ends_with(name, ".WAV")) {
            continue;
        }
        Case c;
        c.path = std::string(dir) + "/" + name;
        c.bpm = 0;
        c.has_bpm = false;
        c.key = Key::Unknown;
        c.has_key = false;
        std::string exp = c.path;
        const size_t dot = exp.find_last_of('.');
        if (dot != std::string::npos) {
            exp = exp.substr(0, dot) + ".expected.json";
            load_expected_json(exp, c);
        }
        out.push_back(c);
    }
    closedir(d);
    return out;
}

static int write_synth_corpus(const char* dir) {
    const float sr = 22050.0f;
    const float bpms[] = {60.0f, 90.0f, 120.0f, 140.0f, 160.0f};
    int n = 0;
    for (int i = 0; i < 5; ++i) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/click_%d.wav", dir, static_cast<int>(bpms[i]));
        std::vector<float> x = click_track(bpms[i], sr, 8.0f);
        if (!write_pcm16_wav(path, x, static_cast<uint32_t>(sr))) {
            std::fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
        char exp[512];
        std::snprintf(exp, sizeof(exp), "%s/click_%d.expected.json", dir, static_cast<int>(bpms[i]));
        FILE* f = std::fopen(exp, "w");
        if (f) {
            std::fprintf(f, "{\"bpm\": %.1f}\n", static_cast<double>(bpms[i]));
            std::fclose(f);
        }
        ++n;
    }
    const float roots[5] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f};
    const char* names[5] = {"C major", "D major", "E major", "F major", "G major"};
    const Key keys[5] = {Key::CMaj, Key::DMaj, Key::EMaj, Key::FMaj, Key::GMaj};
    for (int i = 0; i < 5; ++i) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/triad_%d.wav", dir, i);
        const float f1 = roots[i];
        const float f2 = f1 * std::pow(2.0f, 4.0f / 12.0f);
        const float f3 = f1 * std::pow(2.0f, 7.0f / 12.0f);
        std::vector<float> x = triad(f1, f2, f3, sr, 4.0f);
        if (!write_pcm16_wav(path, x, static_cast<uint32_t>(sr))) {
            return 1;
        }
        char exp[512];
        std::snprintf(exp, sizeof(exp), "%s/triad_%d.expected.json", dir, i);
        FILE* f = std::fopen(exp, "w");
        if (f) {
            std::fprintf(f, "{\"key\": \"%s\"}\n", names[i]);
            std::fclose(f);
        }
        (void)keys;
        ++n;
    }
    std::printf("wrote %d synthetic files to %s\n", n, dir);
    return 0;
}

static void usage() {
    std::fprintf(stderr,
        "Usage:\n"
        "  run_benchmark [options] <dir>\n"
        "\n"
        "Options:\n"
        "  --synth                 Write a synthetic corpus into <dir> then score it.\n"
        "  --no-dp                 Disable Ellis DP.\n"
        "  --clp                   Use CLP chroma.\n"
        "  --json                  Print JSON summary.\n"
        "  --help\n");
}

int main(int argc, char** argv) {
    bool synth = false;
    bool as_json = false;
    Params params;
    const char* dir = 0;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else if (std::strcmp(a, "--synth") == 0) {
            synth = true;
        } else if (std::strcmp(a, "--json") == 0) {
            as_json = true;
        } else if (std::strcmp(a, "--no-dp") == 0) {
            params.enable_dp_refinement = false;
        } else if (std::strcmp(a, "--clp") == 0) {
            params.chroma_method = Params::ChromaMethod::Clp;
            params.use_hpcp = false;
        } else if (a[0] == '-') {
            usage();
            return 64;
        } else if (!dir) {
            dir = a;
        } else {
            return 64;
        }
    }
    if (!dir) {
        usage();
        return 64;
    }
    if (synth) {
        if (write_synth_corpus(dir) != 0) {
            return 1;
        }
    }
    std::vector<Case> cases = scan_dir(dir);
    if (cases.empty()) {
        std::fprintf(stderr, "no wav files in %s\n", dir);
        return 1;
    }

    int n_tempo = 0, ok_tempo = 0, n_key = 0, ok_key = 0, ok_rel = 0;
    validate_params(params);
    for (size_t i = 0; i < cases.size(); ++i) {
        WavData w;
        const WavError e = load_wav(cases[i].path.c_str(), w);
        if (e != WavError::OK) {
            std::fprintf(stderr, "skip %s: %s\n", cases[i].path.c_str(), wav_error_string(e));
            continue;
        }
        AnalysisResult r = analyze(w, params);
        if (cases[i].has_bpm) {
            ++n_tempo;
            const float tol = std::max(0.5f, 0.04f * cases[i].bpm);
            const bool hit = std::fabs(r.tempo.bpm - cases[i].bpm) <= tol
                          || std::fabs(r.tempo.bpm - 2.0f * cases[i].bpm) <= tol
                          || std::fabs(2.0f * r.tempo.bpm - cases[i].bpm) <= tol;
            if (hit) ++ok_tempo;
            std::printf("tempo %s  gt=%.1f est=%.1f %s\n",
                        cases[i].path.c_str(),
                        static_cast<double>(cases[i].bpm),
                        static_cast<double>(r.tempo.bpm),
                        hit ? "OK" : "MISS");
        }
        if (cases[i].has_key) {
            ++n_key;
            const bool hit = r.key.key == cases[i].key;
            const bool rel = relative_ok(r.key.key, cases[i].key);
            if (hit) ++ok_key;
            if (rel) ++ok_rel;
            std::printf("key   %s  gt=%s est=%s %s\n",
                        cases[i].path.c_str(),
                        to_string(cases[i].key),
                        to_string(r.key.key),
                        hit ? "OK" : (rel ? "REL" : "MISS"));
        }
    }
    const float tempo_acc = n_tempo ? (100.0f * static_cast<float>(ok_tempo) / static_cast<float>(n_tempo)) : 0.0f;
    const float key_acc = n_key ? (100.0f * static_cast<float>(ok_key) / static_cast<float>(n_key)) : 0.0f;
    const float rel_acc = n_key ? (100.0f * static_cast<float>(ok_rel) / static_cast<float>(n_key)) : 0.0f;
    if (as_json) {
        std::printf("{\"tempo_n\":%d,\"tempo_ok\":%d,\"tempo_pct\":%.1f,"
                    "\"key_n\":%d,\"key_ok\":%d,\"key_pct\":%.1f,\"key_rel_pct\":%.1f}\n",
                    n_tempo, ok_tempo, static_cast<double>(tempo_acc),
                    n_key, ok_key, static_cast<double>(key_acc), static_cast<double>(rel_acc));
    } else {
        std::printf("tempo accuracy: %d/%d (%.1f%%)\n", ok_tempo, n_tempo, static_cast<double>(tempo_acc));
        std::printf("key accuracy:   %d/%d (%.1f%% strict, %.1f%% relative)\n",
                    ok_key, n_key, static_cast<double>(key_acc), static_cast<double>(rel_acc));
    }
    return 0;
}
