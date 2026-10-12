#include "utest.h"
#include "wav_writer.h"

#include <taktful/api.h>
#include <taktful/c_api.h>
#include <taktful/chroma.h>
#include <taktful/dsp.h>
#include <taktful/key.h>
#include <taktful/onset.h>
#include <taktful/stream.h>
#include <taktful/tempo.h>
#include <taktful/wav_io.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace taktful;
using namespace taktful::test;

UTEST(wav_io, pcm16_mono) {
    const std::vector<float> src = sine(440.0f, 44100.0f, 0.2f);
    const std::string path = tmp_path("taktful_pcm16.wav");
    ASSERT_TRUE(write_pcm16_wav(path.c_str(), src, 44100));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.sample_rate, 44100u);
    ASSERT_EQ(w.channels, 1);
    ASSERT_EQ(w.bits_per_sample, 16);
    ASSERT_TRUE(w.samples.size() > 100);
    ASSERT_TRUE(std::fabs(w.samples[100] - src[100]) < 0.01f);
}

UTEST(wav_io, pcm8_stereo) {
    const std::vector<float> src = sine(220.0f, 8000.0f, 0.1f, 0.8f);
    const std::string path = tmp_path("taktful_pcm8s.wav");
    ASSERT_TRUE(write_pcm_wav_bits(path.c_str(), src, 8000, 8, 2, false));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.channels, 2);
    ASSERT_EQ(w.bits_per_sample, 8);
    ASSERT_TRUE(w.samples.size() == src.size());
}

UTEST(wav_io, pcm24) {
    const std::vector<float> src = sine(100.0f, 16000.0f, 0.05f);
    const std::string path = tmp_path("taktful_pcm24.wav");
    ASSERT_TRUE(write_pcm_wav_bits(path.c_str(), src, 16000, 24, 1, false));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.bits_per_sample, 24);
    ASSERT_TRUE(w.samples.size() == src.size());
}

UTEST(wav_io, pcm32) {
    const std::vector<float> src = sine(100.0f, 16000.0f, 0.05f);
    const std::string path = tmp_path("taktful_pcm32.wav");
    ASSERT_TRUE(write_pcm_wav_bits(path.c_str(), src, 16000, 32, 1, false));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.bits_per_sample, 32);
}

UTEST(wav_io, float32) {
    const std::vector<float> src = sine(330.0f, 22050.0f, 0.1f);
    const std::string path = tmp_path("taktful_f32.wav");
    ASSERT_TRUE(write_pcm_wav_bits(path.c_str(), src, 22050, 32, 1, true));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.bits_per_sample, 32);
    ASSERT_TRUE(std::fabs(w.samples[50] - src[50]) < 1e-5f);
}

UTEST(wav_io, missing_file) {
    WavData w;
    ASSERT_EQ(load_wav("/tmp/taktful_does_not_exist.wav", w), WavError::FileNotFound);
}

UTEST(wav_io, not_a_wav) {
    const char* path = "/tmp/taktful_notwav.bin";
    FILE* f = std::fopen(path, "wb");
    ASSERT_TRUE(f != 0);
    const char* s = "ID3notawavfileXXXX";
    std::fwrite(s, 1, std::strlen(s), f);
    std::fclose(f);
    WavData w;
    ASSERT_EQ(load_wav(path, w), WavError::NotAWav);
}

UTEST(wav_io, empty_file) {
    const char* path = "/tmp/taktful_empty.wav";
    FILE* f = std::fopen(path, "wb");
    ASSERT_TRUE(f != 0);
    std::fclose(f);
    WavData w;
    ASSERT_EQ(load_wav(path, w), WavError::Truncated);
}

UTEST(dsp, power_of_two) {
    ASSERT_TRUE(is_power_of_two(256));
    ASSERT_TRUE(is_power_of_two(4096));
    ASSERT_FALSE(is_power_of_two(0));
    ASSERT_FALSE(is_power_of_two(3));
    ASSERT_FALSE(is_power_of_two(4095));
}

UTEST(dsp, fft_dc) {
    const uint32_t n = 512;
    std::vector<float> x(n, 1.0f);
    std::vector<float> mag(n / 2 + 1, 0.0f);
    rfft_mag(x.data(), mag.data(), n);
    ASSERT_TRUE(mag[0] > mag[1] * 10.0f);
    ASSERT_TRUE(mag[0] > 400.0f);
}

UTEST(dsp, fft_impulse) {
    const uint32_t n = 256;
    std::vector<float> x(n, 0.0f);
    x[0] = 1.0f;
    std::vector<float> mag(n / 2 + 1, 0.0f);
    rfft_mag(x.data(), mag.data(), n);
    for (uint32_t k = 0; k < mag.size(); ++k) {
        ASSERT_TRUE(std::fabs(mag[k] - 1.0f) < 1e-4f);
    }
}

UTEST(dsp, fft_sine_bin) {
    const uint32_t n = 1024;
    const uint32_t k0 = 8;
    std::vector<float> x(n);
    for (uint32_t i = 0; i < n; ++i) {
        x[i] = std::sin(2.0f * 3.14159265f * static_cast<float>(k0) * static_cast<float>(i) / static_cast<float>(n));
    }
    std::vector<float> mag(n / 2 + 1, 0.0f);
    rfft_mag(x.data(), mag.data(), n);
    uint32_t peak = 0;
    float best = -1.0f;
    for (uint32_t k = 0; k < mag.size(); ++k) {
        if (mag[k] > best) {
            best = mag[k];
            peak = k;
        }
    }
    ASSERT_EQ(peak, k0);
}

UTEST(dsp, fft_all_sizes) {
    const uint32_t sizes[] = {256, 512, 1024, 2048, 4096, 8192, 16384};
    for (uint32_t s = 0; s < 7; ++s) {
        const uint32_t n = sizes[s];
        std::vector<float> x(n, 0.0f);
        x[0] = 1.0f;
        std::vector<float> mag(n / 2 + 1);
        rfft_mag(x.data(), mag.data(), n);
        ASSERT_TRUE(std::fabs(mag[3] - 1.0f) < 1e-3f);
    }
}

UTEST(dsp, window_hann_ends) {
    const uint32_t n = 64;
    std::vector<float> w(n);
    make_window(WindowType::Hann, w.data(), n);
    ASSERT_TRUE(std::fabs(w[0]) < 1e-5f);
    ASSERT_TRUE(std::fabs(w[n - 1]) < 1e-5f);
    ASSERT_TRUE(w[n / 2] > 0.9f);
}

UTEST(dsp, apply_window_and_rms) {
    const uint32_t n = 7;
    std::vector<float> x(n, 2.0f);
    std::vector<float> w(n, 0.5f);
    apply_window(x.data(), w.data(), n);
    for (uint32_t i = 0; i < n; ++i) {
        ASSERT_TRUE(std::fabs(x[i] - 1.0f) < 1e-6f);
    }
    const float rms = signal_rms(x.data(), n);
    ASSERT_TRUE(std::fabs(rms - 1.0f) < 1e-5f);
}

UTEST(dsp, downsample) {
    std::vector<float> in(44100, 1.0f);
    std::vector<float> out;
    downsample_linear(in.data(), 44100, 44100, 11025, out);
    ASSERT_TRUE(out.size() >= 11020 && out.size() <= 11026);
    ASSERT_TRUE(std::fabs(out[10] - 1.0f) < 1e-4f);
}

UTEST(dsp, hz_to_midi_a4) {
    ASSERT_TRUE(std::fabs(hz_to_midi(440.0f, 440.0f) - 69.0f) < 1e-4f);
    ASSERT_EQ(midi_to_pitch_class(69.0f), 9);
    ASSERT_EQ(midi_to_pitch_class(60.0f), 0);
}

UTEST(onset, impulse_train) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.target_sample_rate = 11025;
    const float sr = 11025.0f;
    std::vector<float> x = click_track(120.0f, sr, 4.0f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, 1024, 256, WindowType::Hann);
    std::vector<float> env = compute_onset_envelope(stft.mag, true);
    ASSERT_TRUE(env.size() > 10);
    float mx = 0.0f;
    float mean = 0.0f;
    for (size_t i = 0; i < env.size(); ++i) {
        mean += env[i];
        if (env[i] > mx) mx = env[i];
    }
    mean /= static_cast<float>(env.size());
    ASSERT_TRUE(mx > mean * 2.0f);
}

UTEST(onset, white_noise_no_huge_peaks) {
    std::vector<float> x(11025 * 2);
    uint32_t seed = 1;
    for (size_t i = 0; i < x.size(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        x[i] = (static_cast<float>(seed & 0xffff) / 65535.0f) * 2.0f - 1.0f;
    }
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, 1024, 512, WindowType::Hann);
    std::vector<float> env = compute_onset_envelope(stft.mag, false);
    double mean = 0.0;
    double var = 0.0;
    for (size_t i = 0; i < env.size(); ++i) mean += env[i];
    mean /= static_cast<double>(env.size());
    for (size_t i = 0; i < env.size(); ++i) {
        const double d = env[i] - mean;
        var += d * d;
    }
    var /= static_cast<double>(env.size());
    const float stdv = static_cast<float>(std::sqrt(var));
    int outliers = 0;
    for (size_t i = 0; i < env.size(); ++i) {
        if (env[i] > mean + 6.0 * stdv) ++outliers;
    }
    ASSERT_TRUE(outliers < static_cast<int>(env.size()) / 10);
}

UTEST(tempo, click_120) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.target_sample_rate = 11025;
    p.enable_dp_refinement = false;
    const float sr = 11025.0f;
    std::vector<float> x = click_track(120.0f, sr, 8.0f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<float> env = compute_onset_envelope(stft.mag, true);
    const float hop_sec = static_cast<float>(p.hop_size) / sr;
    TempoResult r = estimate_tempo(env, hop_sec, p);
    ASSERT_TRUE(std::fabs(r.bpm - 120.0f) < 1.0f);
}

UTEST(tempo, click_60) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.target_sample_rate = 11025;
    p.enable_dp_refinement = false;
    const float sr = 11025.0f;
    std::vector<float> x = click_track(60.0f, sr, 10.0f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<float> env = compute_onset_envelope(stft.mag, true);
    TempoResult r = estimate_tempo(env, static_cast<float>(p.hop_size) / sr, p);
    ASSERT_TRUE(std::fabs(r.bpm - 60.0f) < 1.5f || std::fabs(r.bpm - 120.0f) < 1.5f);
}

UTEST(tempo, click_160) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.enable_dp_refinement = false;
    const float sr = 11025.0f;
    std::vector<float> x = click_track(160.0f, sr, 8.0f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<float> env = compute_onset_envelope(stft.mag, true);
    TempoResult r = estimate_tempo(env, static_cast<float>(p.hop_size) / sr, p);
    ASSERT_TRUE(std::fabs(r.bpm - 160.0f) < 2.0f || std::fabs(r.bpm - 80.0f) < 2.0f);
}

UTEST(tempo, click_90_and_140) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.enable_dp_refinement = false;
    const float sr = 11025.0f;
    {
        std::vector<float> x = click_track(90.0f, sr, 8.0f);
        StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                       11025, p.fft_size, p.hop_size, WindowType::Hann);
        TempoResult r = estimate_tempo(compute_onset_envelope(stft.mag, true),
                                       static_cast<float>(p.hop_size) / sr, p);
        ASSERT_TRUE(std::fabs(r.bpm - 90.0f) < 1.5f);
    }
    {
        std::vector<float> x = click_track(140.0f, sr, 8.0f);
        StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                       11025, p.fft_size, p.hop_size, WindowType::Hann);
        TempoResult r = estimate_tempo(compute_onset_envelope(stft.mag, true),
                                       static_cast<float>(p.hop_size) / sr, p);
        ASSERT_TRUE(std::fabs(r.bpm - 140.0f) < 2.0f || std::fabs(r.bpm - 70.0f) < 2.0f);
    }
}

UTEST(tempo, silent) {
    Params p;
    p.enable_dp_refinement = false;
    std::vector<float> env(200, 0.0f);
    TempoResult r = estimate_tempo(env, 0.05f, p);
    ASSERT_TRUE(r.bpm == 0.0f);
}

UTEST(chroma, sine_a4) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.use_hpcp = true;
    p.apply_pre_filter = false;
    const float sr = 11025.0f;
    std::vector<float> x = sine(440.0f, sr, 2.0f, 0.8f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<std::vector<float> > ch = compute_chroma(stft.mag, p.fft_size, 11025, p);
    std::vector<float> pcp = aggregate_pcp(ch);
    int peak = 0;
    for (int i = 1; i < 12; ++i) {
        if (pcp[static_cast<size_t>(i)] > pcp[static_cast<size_t>(peak)]) peak = i;
    }
    ASSERT_EQ(peak, 9);
}

UTEST(chroma, sine_c5) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.use_hpcp = true;
    p.apply_pre_filter = false;
    const float sr = 11025.0f;
    std::vector<float> x = sine(523.251f, sr, 2.0f, 0.8f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<std::vector<float> > ch = compute_chroma(stft.mag, p.fft_size, 11025, p);
    std::vector<float> pcp = aggregate_pcp(ch);
    int peak = 0;
    for (int i = 1; i < 12; ++i) {
        if (pcp[static_cast<size_t>(i)] > pcp[static_cast<size_t>(peak)]) peak = i;
    }
    ASSERT_EQ(peak, 0);
}

UTEST(chroma, off_tune_a4) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.apply_pre_filter = false;
    const float sr = 11025.0f;
    std::vector<float> x = sine(445.0f, sr, 2.0f, 0.8f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<float> pcp = aggregate_pcp(compute_chroma(stft.mag, p.fft_size, 11025, p));
    int peak = 0;
    for (int i = 1; i < 12; ++i) {
        if (pcp[static_cast<size_t>(i)] > pcp[static_cast<size_t>(peak)]) peak = i;
    }
    ASSERT_EQ(peak, 9);
}

UTEST(key, c_major_triad) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.apply_pre_filter = false;
    p.enable_dp_refinement = false;
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.bits_per_sample = 16;
    w.samples = triad(261.63f, 329.63f, 392.00f, 11025.0f, 3.0f);
    AnalysisResult r = analyze(w, p);
    ASSERT_TRUE(r.key.key == Key::CMaj || r.key.key == Key::AMin);
}

UTEST(key, a_minor_triad) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.apply_pre_filter = false;
    p.enable_dp_refinement = false;
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.bits_per_sample = 16;
    w.samples = triad(220.00f, 261.63f, 329.63f, 11025.0f, 3.0f);
    AnalysisResult r = analyze(w, p);
    ASSERT_TRUE(r.key.key == Key::AMin || r.key.key == Key::CMaj);
}

UTEST(key, all_major_triads) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.apply_pre_filter = false;
    p.enable_dp_refinement = false;
    const float roots[12] = {
        261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
        369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f
    };
    int ok = 0;
    for (int k = 0; k < 12; ++k) {
        const float f1 = roots[k];
        const float f2 = f1 * std::pow(2.0f, 4.0f / 12.0f);
        const float f3 = f1 * std::pow(2.0f, 7.0f / 12.0f);
        WavData w;
        w.sample_rate = 11025;
        w.channels = 1;
        w.samples = triad(f1, f2, f3, 11025.0f, 2.5f);
        AnalysisResult r = analyze(w, p);
        const int expected = k;
        const int relative_minor = (k + 9) % 12 + 12;
        if (static_cast<int>(r.key.key) == expected || static_cast<int>(r.key.key) == relative_minor) {
            ++ok;
        }
    }
    ASSERT_TRUE(ok >= 10);
}

UTEST(key, all_minor_triads) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.apply_pre_filter = false;
    p.enable_dp_refinement = false;
    const float roots[12] = {
        261.63f, 277.18f, 293.66f, 311.13f, 329.63f, 349.23f,
        369.99f, 392.00f, 415.30f, 440.00f, 466.16f, 493.88f
    };
    int ok = 0;
    for (int k = 0; k < 12; ++k) {
        const float f1 = roots[k];
        const float f2 = f1 * std::pow(2.0f, 3.0f / 12.0f);
        const float f3 = f1 * std::pow(2.0f, 7.0f / 12.0f);
        WavData w;
        w.sample_rate = 11025;
        w.channels = 1;
        w.samples = triad(f1, f2, f3, 11025.0f, 2.5f);
        AnalysisResult r = analyze(w, p);
        const int expected = 12 + k;
        const int relative_major = k;
        if (static_cast<int>(r.key.key) == expected || static_cast<int>(r.key.key) == relative_major) {
            ++ok;
        }
    }
    ASSERT_TRUE(ok >= 10);
}

UTEST(api, silent_file) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples.assign(11025 * 5, 0.0f);
    AnalysisResult r = analyze(w);
    ASSERT_EQ(r.tempo.bpm, 0.0f);
    ASSERT_TRUE(r.key.key == Key::Unknown);
}

UTEST(api, to_string_and_camelot) {
    ASSERT_TRUE(std::strcmp(to_string(Key::CMaj), "C major") == 0);
    ASSERT_TRUE(std::strcmp(to_string(Key::AMin), "A minor") == 0);
    ASSERT_TRUE(std::strcmp(to_camelot(Key::CMaj), "8B") == 0);
    ASSERT_TRUE(std::strcmp(to_camelot(Key::AMin), "8A") == 0);
}

UTEST(api, compute_features) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples = sine(440.0f, 11025.0f, 1.0f);
    IntermediateFeatures f = compute_features(w);
    ASSERT_TRUE(f.onset_envelope.size() > 0);
    ASSERT_TRUE(f.chroma.size() > 0);
    ASSERT_EQ(f.chroma[0].size(), 12u);
}

UTEST(api, version) {
    ASSERT_EQ(TAKTFUL_VERSION_MAJOR, 1);
    ASSERT_TRUE(std::strcmp(TAKTFUL_VERSION_STRING, "1.0.1") == 0);
}

UTEST(wav_io, load_mem_roundtrip) {
    const std::vector<float> src = sine(440.0f, 8000.0f, 0.1f);
    const std::string path = tmp_path("taktful_mem.wav");
    ASSERT_TRUE(write_pcm16_wav(path.c_str(), src, 8000));
    FILE* f = std::fopen(path.c_str(), "rb");
    ASSERT_TRUE(f != 0);
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::rewind(f);
    std::vector<unsigned char> buf(static_cast<size_t>(sz));
    ASSERT_TRUE(std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);
    WavData w;
    ASSERT_EQ(load_wav_mem(buf.data(), buf.size(), w), WavError::OK);
    ASSERT_EQ(w.sample_rate, 8000u);
    ASSERT_TRUE(w.samples.size() == src.size());
}

UTEST(wav_io, rf64) {
    const std::vector<float> src = sine(220.0f, 8000.0f, 0.2f);
    const std::string path = tmp_path("taktful_rf64.wav");
    ASSERT_TRUE(write_rf64_pcm16(path.c_str(), src, 8000));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    ASSERT_EQ(w.sample_rate, 8000u);
    ASSERT_TRUE(w.samples.size() == src.size());
    ASSERT_TRUE(std::fabs(w.samples[20] - src[20]) < 0.02f);
}

UTEST(stream, click_chunks) {
    Params p;
    p.fft_size = 1024;
    p.hop_size = 256;
    p.target_sample_rate = 11025;
    p.enable_dp_refinement = false;
    const float sr = 11025.0f;
    std::vector<float> x = click_track(120.0f, sr, 8.0f);
    StreamAnalyzer s(p);
    const uint32_t chunk = 512;
    for (uint32_t i = 0; i < x.size(); i += chunk) {
        uint32_t n = chunk;
        if (i + n > x.size()) n = static_cast<uint32_t>(x.size() - i);
        s.push(x.data() + i, n, 11025);
    }
    AnalysisResult r = s.finish();
    ASSERT_TRUE(std::fabs(r.tempo.bpm - 120.0f) < 2.0f);
}

UTEST(api, analyze_buffer_and_sections) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples = sine(440.0f, 11025.0f, 2.5f);
    Params p;
    p.enable_dp_refinement = false;
    p.apply_pre_filter = false;
    AnalysisResult a = analyze_buffer(w.samples.data(), static_cast<uint32_t>(w.samples.size()), 11025, p);
    ASSERT_TRUE(a.key.key != Key::Unknown);
    std::vector<AnalysisResult> secs = analyze_sections(w, 1.0f, p);
    ASSERT_TRUE(secs.size() >= 2);
}

UTEST(chroma, clp_a4) {
    Params p;
    p.fft_size = 4096;
    p.hop_size = 2048;
    p.chroma_method = Params::ChromaMethod::Clp;
    p.use_hpcp = false;
    p.apply_pre_filter = false;
    const float sr = 11025.0f;
    std::vector<float> x = sine(440.0f, sr, 2.0f, 0.8f);
    StftFrames stft = compute_stft(x.data(), static_cast<uint32_t>(x.size()),
                                   11025, p.fft_size, p.hop_size, WindowType::Hann);
    std::vector<float> pcp = aggregate_pcp(compute_chroma(stft.mag, p.fft_size, 11025, p));
    int peak = 0;
    for (int i = 1; i < 12; ++i) {
        if (pcp[static_cast<size_t>(i)] > pcp[static_cast<size_t>(peak)]) peak = i;
    }
    ASSERT_EQ(peak, 9);
}

UTEST(c_api, analyze_buffer) {
    std::vector<float> x = triad(261.63f, 329.63f, 392.00f, 11025.0f, 3.0f);
    taktful_params p;
    taktful_params_default(&p);
    p.enable_dp_refinement = 0;
    p.apply_pre_filter = 0;
    taktful_result r;
    ASSERT_EQ(taktful_analyze_buffer(x.data(), static_cast<uint32_t>(x.size()), 11025, &p, &r), TAKTFUL_OK);
    ASSERT_TRUE(r.key == TAKTFUL_KEY_C_MAJ || r.key == TAKTFUL_KEY_A_MIN);
    ASSERT_TRUE(std::strcmp(taktful_version(), "1.0.1") == 0);
    taktful_result_free(&r);
}

UTEST_MAIN()
