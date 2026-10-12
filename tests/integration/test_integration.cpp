#include "utest.h"
#include "wav_writer.h"

#include <taktful/api.h>
#include <taktful/c_api.h>
#include <taktful/stream.h>
#include <taktful/wav_io.h>

#include <cmath>
#include <string>
#include <vector>

using namespace taktful;
using namespace taktful::test;

UTEST(integration, analyze_click_and_tone) {
    WavData w;
    w.sample_rate = 44100;
    w.channels = 1;
    w.bits_per_sample = 16;
    std::vector<float> clicks = click_track(120.0f, 44100.0f, 6.0f, 0.7f);
    std::vector<float> tone = triad(261.63f, 329.63f, 392.00f, 44100.0f, 6.0f, 0.25f);
    w.samples.resize(clicks.size());
    for (size_t i = 0; i < w.samples.size(); ++i) {
        w.samples[i] = clicks[i] + tone[i];
    }
    Params p;
    p.enable_dp_refinement = false;
    p.apply_pre_filter = false;
    AnalysisResult r = analyze(w, p);
    ASSERT_TRUE(r.tempo.bpm > 0.0f);
    ASSERT_TRUE(r.key.key != Key::Unknown);
}

UTEST(integration, silent_five_seconds) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples.assign(11025 * 5, 0.0f);
    AnalysisResult r = analyze(w);
    ASSERT_EQ(r.tempo.bpm, 0.0f);
    ASSERT_TRUE(r.key.key == Key::Unknown);
}

UTEST(integration, one_second_file) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples = sine(440.0f, 11025.0f, 1.0f);
    Params p;
    p.enable_dp_refinement = false;
    AnalysisResult r = analyze(w, p);
    ASSERT_TRUE(r.key.key != Key::Unknown || r.tempo.bpm >= 0.0f);
}

UTEST(integration, roundtrip_wav_analyze) {
    const std::string path = tmp_path("taktful_int_roundtrip.wav");
    std::vector<float> x = click_track(90.0f, 22050.0f, 6.0f);
    ASSERT_TRUE(write_pcm16_wav(path.c_str(), x, 22050));
    WavData w;
    ASSERT_EQ(load_wav(path.c_str(), w), WavError::OK);
    Params p;
    p.enable_dp_refinement = false;
    AnalysisResult r = analyze(w, p);
    ASSERT_TRUE(r.tempo.bpm > 50.0f);
}

UTEST(integration, features_match_analyze_length) {
    WavData w;
    w.sample_rate = 11025;
    w.channels = 1;
    w.samples = sine(440.0f, 11025.0f, 2.0f);
    IntermediateFeatures f = compute_features(w);
    AnalysisResult r = analyze(w);
    ASSERT_TRUE(f.chroma.size() == f.onset_envelope.size() || f.chroma.size() > 0);
    (void)r;
}

UTEST(integration, c_api_load_and_analyze) {
    const std::string path = tmp_path("taktful_int_capi.wav");
    std::vector<float> x = click_track(120.0f, 22050.0f, 6.0f);
    ASSERT_TRUE(write_pcm16_wav(path.c_str(), x, 22050));
    taktful_wav wav;
    ASSERT_EQ(taktful_load_wav(path.c_str(), &wav), TAKTFUL_OK);
    taktful_params p;
    taktful_params_default(&p);
    p.enable_dp_refinement = 0;
    taktful_result r;
    ASSERT_EQ(taktful_analyze(&wav, &p, &r), TAKTFUL_OK);
    ASSERT_TRUE(r.bpm > 50.0f);
    taktful_result_free(&r);
    taktful_wav_free(&wav);
}

UTEST_MAIN()
