# Lightweight C++ Audio Analysis Library — Development Plan

**Project:** Musical Key & BPM detection from WAV files
**Target audience:** Embedded C++ developers, handheld-console music tracker project
**Document type:** Engineering design + research summary + roadmap
**License:** MIT (planned)
**C++ standard:** C++14
**Build system:** CMake (single Makefile fallback)
**Primary target:** Linux handhelds (e.g. ARM Cortex-A class, Anbernic / Miyoo / AYN / Steam Deck class). Cross-platform build supported on desktop Linux / macOS / Windows for development.
**Author:** Plan v0.1
**Status:** Draft for review

---

## 1. Executive Summary

This document defines the engineering plan for a small, MIT-licensed C++14 library that, given a WAV file, returns (a) the most likely musical key (with a ranked list of alternatives) and (b) an estimated tempo in BPM. The library is intended for embedding inside a music tracker for Linux handheld consoles; size and memory footprint are the dominant design constraints, with single-digit MB peak RAM and zero external runtime dependencies.

The plan is grounded in a survey of five open-source audio-analysis libraries (libKeyFinder, aubio, Essentia, TarsosDSP, Vamp/QM-Vamp) and a study of the underlying published algorithms (Krumhansl–Schmuckler key finding, Temperley's modifications, Harmonic Pitch Class Profile (HPCP) chroma, Ellis dynamic-programming beat tracking, and various onset-detection methods). All surveyed libraries are GPL/AGPL-licensed; their code will not be reused. Their *algorithms* — which are not copyrightable in the United States or EU when implemented from the underlying papers — will be reimplemented cleanly from first principles, using the original academic publications as the source of truth rather than the libraries' source code.

The proposed pipeline is: **WAV decode → downsample → short-time Fourier transform → onset envelope → tempo hypothesis (autocorrelation + Ellis DP beat tracking) → key hypothesis (HPCP chroma aggregation → Krumhansl–Schmuckler with Temperley modifications → ranked key outputs)**. All DSP operates on `float` (32-bit) sample buffers; FFT is provided by a vendored radix-2 Cooley–Tukey implementation that fits in under 200 lines of C++ and supports power-of-two sizes 256–16384. The library exposes a single, dependency-free static library `libtaktful` plus three CLI tools (`analyze_wav`, `bpm_probe`, `key_probe`) for testing and tuning.

Phased delivery is targeted across four milestones (MVP → v0.5 → v1.0 → v1.x), with the MVP covering WAV load + single-pass tempo via autocorrelation within two weeks of work, and v1.0 delivering the full key + tempo pipeline with validated accuracy against the MIREX 2018 tempo dataset and the MTG Key dataset. Total estimated effort: 6–10 person-weeks of focused work.

The remainder of this document expands on the research, algorithm choices, architecture, validation plan, roadmap, and licensing audit notes.

---

## 2. Goals & Non-Goals

### 2.1 Goals

1. **Correctness over cleverness.** The library should produce key and tempo estimates that agree with the published state-of-the-art for the genres it targets (electronic / tracker music, plus "unknown" mixed content) within published accuracy ranges (MIREX tempo accuracy ≥ 80% on the Extended Ballroom dataset; key accuracy ≥ 60% on the MTG Key dataset for EDM, which matches the published performance of non-ML systems).
2. **Lightweight.** Compiled object size ≤ 200 KB stripped, single static library, ≤ 50 KB working set during analysis (excluding the loaded audio buffer itself).
3. **Zero runtime dependencies.** No libsndfile, no FFTW, no libCURL, no Boost. Only the C/C++ standard library and a handful of vendored single-file headers.
4. **MIT-licensed.** Reusable in any proprietary or open-source project, including the user's tracker, without copyleft contamination.
5. **Embeddable in a music tracker.** Public API supports both a high-level "give me the answer" entry point and a low-level "give me the per-frame chroma / onset envelope" entry point for visualisation inside the tracker UI.
6. **Testable from the command line.** Three reference CLI tools expose every stage of the pipeline so the user can iterate on parameters without writing code.
7. **No machine learning, no neural networks.** Pure signal-processing + heuristic scoring. This is a hard constraint imposed by the user, both for licensing reasons (training data often has unclear licensing) and for runtime cost reasons on handhelds.

### 2.2 Non-Goals

1. **Real-time analysis.** The library is offline-only in v1.0. A streaming API may be added in v1.x but is not committed.
2. **Chord transcription, beat tracking with measure numbering, structural segmentation.** Only global tempo and global key (with ranked alternatives).
3. **Source separation.** The library analyses the mixed audio signal as-is.
4. **Microtonal or non-12-TET key estimation.** Standard 12-tone equal temperament only.
5. **Per-instrument analysis.** No instrument recognition, no transcription.
6. **Multi-format input.** Only WAV (PCM 8/16/24/32-bit signed integer, 32-bit float) is supported in v1.0. FLAC / Ogg / MP3 decoding is the host application's responsibility.
7. **Training, tuning, or model distribution.** All parameters are constants baked into the library or passed via a `Params` struct at runtime.
8. **GPU compute.** ARM NEON kernels for FFT / magnitude / window / RMS are implemented; GPU offload is not.

---

## 3. Background — Audio Analysis Fundamentals

This section gives the reader the minimum DSP background needed to understand the rest of the plan. It is deliberately brief; readers familiar with the topic can skip to §4.

### 3.1 From samples to spectrum

A PCM WAV file stores a stream of integer (or float) samples representing the audio waveform at a fixed sample rate (commonly 44.1 kHz or 48 kHz). To analyse frequency content, we window the stream into overlapping frames (e.g. 4096 samples, hop size 2048) and apply a Discrete Fourier Transform (DFT) to each frame, producing a short-time spectrum. The magnitude of each output bin tells us how much energy is present at the frequency corresponding to that bin, in the time-window covered by that frame.

The Fast Fourier Transform (FFT) is the standard O(N log N) algorithm for computing a DFT. We will use a radix-2 Cooley–Tukey FFT, which requires the FFT size N to be a power of two. Real-to-complex FFT (RFFT) is a 2× optimisation for real input which we will use because audio samples are real.

### 3.2 Spectral flux and onset detection

An *onset* is the start of a musical event — a drum hit, a note attack. Onset detection works by computing a per-frame *novelty function* that peaks when new spectral content appears. The simplest and most effective novelty function for our use case is **spectral flux**: the sum of positive differences in magnitude between consecutive frames.

Given a sequence of magnitude spectra $|X_t(k)|$ for frame $t$ and bin $k$, spectral flux is:

$$ SF(t) = \sum_{k} \max(0, |X_t(k)| - |X_{t-1}(k)|) $$

Peaks in $SF(t)$ correspond to onsets. To find peaks we apply a simple half-wave-rectified moving-average smoother and pick local maxima above an adaptive threshold (often the mean plus one standard deviation of $SF$ over the song).

### 3.3 Tempo from onsets

Given a sequence of onset times, tempo can be estimated by:

1. **Autocorrelation** of the onset envelope (interpolated to a fine grid). The lag with the highest autocorrelation corresponds to the beat period. This is robust but ambiguous (octave errors — half / double tempo).
2. **Comb filter bank**. Pass the onset envelope through a bank of resonant comb filters tuned to different periods; pick the filter with the highest output energy. Conceptually similar to autocorrelation but more tunable.
3. **Dynamic programming beat tracking** (Ellis 2007). Estimate global tempo first (via autocorrelation), then use DP to find the optimal beat sequence given that tempo. Best for *beat* detection; tempo falls out of step 1.

We will use a combination: autocorrelation for tempo, with an optional Ellis-DP refinement in v1.0 if time permits.

### 3.4 Chroma and key detection

*Chroma* is a 12-dimensional feature vector representing the energy present at each of the 12 pitch classes (C, C#, D, ..., B) in a frame, computed by aggregating the spectral energy in bins whose frequency falls within each pitch class's frequency range (across all octaves). Summing chroma over the whole song gives a **pitch class profile (PCP)**.

The **Krumhansl–Schmuckler** key-finding algorithm correlates the PCP against 24 pre-defined tonal hierarchy vectors (one per major / minor key, derived from perceptual experiments by Krumhansl 1982) and picks the key whose correlation is highest. Temperley (1999) proposed minor modifications to the reference profiles that improve accuracy on modern music; we will use the Temperley profiles by default.

A more modern chroma variant is the **Harmonic Pitch Class Profile (HPCP)** (Gómez 2006), which weights spectral peaks by their magnitude and only considers local maxima (rather than summing all bins in a pitch class range). HPCP tends to be more robust on polyphonic music than naive PCP and is what libKeyFinder and Essentia use. We will adopt HPCP as our default chroma extractor.

---

## 4. Research — Existing Open-Source Implementations

This section surveys five relevant open-source audio analysis libraries. For each we record: license, scope, dependencies, algorithm summary, and a verdict on whether we can reuse any of its code, design, or merely its algorithm references.

### 4.1 libKeyFinder

- **Repo:** `github.com/ibsh/libKeyFinder`
- **License:** GPL-3.0-or-later (confirmed via Homebrew formulae and the project README)
- **Language:** C++11
- **Scope:** Musical key detection only (no tempo / BPM)
- **Dependencies:** FFTW3 (LGPL), KissFFT optional, Qt for the GUI front-end (but the lib itself is Qt-free)
- **Algorithm summary:** HPCP-style chroma via a Constant-Q transform (using FFTW3 as the underlying FFT). Chroma is computed frame-by-frame, then collapsed to a single tonal profile, which is correlated against all 24 Krumhansl–Schmuckler / Temperley reference profiles. The winning key, plus a confidence score, is returned.
- **What we can take:** The *idea* of using HPCP + Temperley profiles + cosine similarity. We must reimplement from scratch; we cannot even read the source for "reference" while remaining legally clean. **Clean-room rule:** the implementer of our key detection module must NOT read libKeyFinder's source code; they should read the underlying papers (Gómez 2006 HPCP, Temperley 1999, Krumhansl 1990).
- **Footprint:** ~600 KB compiled with FFTW3 linked. Without FFTW3, ~200 KB. Too large for our target as-is, but the algorithm is portable.

### 4.2 aubio

- **Repo:** `aubio.org`
- **License:** GPL-3.0+ (header files declare it explicitly; well-established)
- **Language:** C99 (with Python bindings)
- **Scope:** Pitch detection, onset detection, tempo / beat tracking, source separation, MFCCs. A real "swiss army knife" for MIR.
- **Dependencies:** None for the C core (it ships its own FFT or uses FFTW). Has optional samplerate / sndfile dependencies.
- **Algorithm summary:** Onset detection via spectral flux (default) or complex-domain; tempo via comb-filter bank on the onset envelope; pitch via YIN or McLeod Pitch Method. Highly modular.
- **What we can take:** The *approach* (spectral flux → comb filter tempo) is well documented in the literature and in Bello 2005; we can reimplement cleanly from those. **Clean-room rule:** the implementer of our onset / tempo modules must NOT read aubio's source. Reference: Bello 2005 "A tutorial on onset detection in music signals" (this paper is the source of truth, not the aubio code).
- **Footprint:** ~400 KB compiled. Small enough on its own, but the GPL license blocks inclusion.

### 4.3 Essentia (MTG, UPF Barcelona)

- **Repo:** `github.com/MTG/essentia`
- **License:** AGPL-3.0 for non-commercial use; commercial license available on request from MTG.
- **Language:** C++ (with Python bindings)
- **Scope:** Comprehensive MIR: tempo, key, chord, beat, structure, MFCC, lots of ML-based extractors in `essentia TSA`/`TensorFlow` modules.
- **Dependencies:** Heavy. Eigen for many algorithms, libTag, FFmpeg (optional), libsndfile, plus Gaia for ML model storage. The "core" is somewhat lighter but still pulls in Eigen.
- **Algorithm summary:** Multiple algorithm classes — the key estimator is `Key` (uses HPCP + Krumhansl–Schmuckler, similar to libKeyFinder). Tempo is `TempoTap` / `TempoTapMax` (DP-based, multiple hypothesis tracking). Modern Essentia also has TempoCNN (a CNN model) — explicitly out of scope for us.
- **What we can take:** Architectural ideas only — the concept of a "chain" of `Algorithm` objects with typed input/output ports is a nice design pattern we may borrow at the API level (no code). **Clean-room rule:** the implementer of our key/tempo modules must NOT read Essentia source. Reference the underlying papers instead.
- **Footprint:** > 5 MB compiled (with all algorithms). Way too large for our target.

### 4.4 TarsosDSP

- **Repo:** `github.com/JorenSix/TarsosDSP`
- **License:** GPL-3.0+
- **Language:** Java (cross-platform via JVM, but JIT not realistic on handheld Linux)
- **Scope:** Pitch detection, onset, tempo, audio I/O, feature extraction. Particularly well-known for the `PitchProcessor` with multiple algorithms (YIN, McLeod, dynamic wavelet).
- **Algorithm summary:** Onset via spectral flux or complex-domain; tempo via simple beat-following on the onset envelope; pitch via YIN / McLeod. Pure-Java implementation makes the algorithms easy to read for understanding (though we cannot copy the code).
- **What we can take:** The cleanest implementation of the *percussion detection → tempo* pipeline I have seen in OSS. The high-level block diagram is excellent reference material. **Clean-room rule:** implementer may look at TarsosDSP's class layout (architectural) for understanding the *shape* of the problem, but must not copy code; implementations should be derived from Bello 2005 and Ellis 2007.
- **Footprint:** N/A for our use case (JVM).

### 4.5 Vamp SDK + QM Vamp Plugins

- **Repo:** `github.com/c4dm/qm-vamp-plugins`
- **License:** Vamp SDK itself is X11 + BSD-3-Clause (MIT-compatible!); QM Vamp Plugins are GPL-2.0+ for the plugin code, but the SDK is permissively licensed.
- **Language:** C++
- **Scope:** Plugin framework for feature extraction; QM plugins provide chroma, key, tempo, onset, etc.
- **Algorithm summary:** QM Key uses chroma + Krumhansl–Schmuckler. QM Tempo uses a comb-filter bank on the onset envelope (similar to aubio). Well-cited implementations; the underlying papers are the reference.
- **What we can take:** The Vamp SDK is MIT-equivalent licensed, so we *could* reuse its plugin framework. However, the plugin framework is overkill for our embedded use case (it's designed for DAWs like Sonic Visualiser). The QM *plugin source* is GPL and cannot be reused. **Decision:** do not depend on Vamp at all — it adds complexity without value for the embedded use case. Reference the QM key algorithm via Mauch's paper "Beatles compared: structural comparison of audio recordings" or Dixon's MA approach.
- **Footprint:** N/A (we won't use it).

### 4.6 Summary of OSS audit

The audit confirms what the user suspected: the key algorithms we need (HPCP chroma, Krumhansl–Schmuckler / Temperley key finding, spectral flux onset, comb filter or DP tempo) are well-established in the literature going back 20–40 years. **None of the existing library code is MIT-compatible** — libKeyFinder, aubio, Essentia, and TarsosDSP are all GPL/AGPL, and the QM Vamp plugins are GPL even though the Vamp SDK is BSD/X11. We must reimplement from the published papers, not from the library code.

This is not as bad as it sounds: the algorithms are typically 200–500 lines of code each, the papers are clearly written, and there are multiple independent implementations to cross-check against (we can compare the *outputs* of our reimplementation against the *outputs* of GPL tools running on the same test WAV, which is fine — running the tools does not contaminate our code).

---

## 5. Algorithm Comparison Matrix

The matrix below compares each candidate algorithm along the dimensions that matter for our handheld target: implementation complexity, CPU cost, memory cost, accuracy on our target genres (electronic / mixed), and license-cleanliness (whether the algorithm is freely re-implementable from public papers, with no GPL code touching our codebase).

| Stage | Candidate algorithm | Source paper / library | License of original code | Impl. complexity | CPU per second of audio | Memory peak | Accuracy (typical, electronic) | Verdict |
|---|---|---|---|---|---|---|---|---|
| **FFT** | Radix-2 Cooley–Tukey RFFT | Cooley & Tukey 1965 | Public domain algorithm | Low (200 LOC) | ~1% on Cortex-A7 | Negligible | N/A | ✅ Adopt |
| **FFT** | FFTW3 "pocketfft" | FFTW (Matteo Frigo) | GPL — but pocketfft submodule is BSD-3-Clause | Medium (vendoring pocketfft is OK) | Comparable | Comparable | N/A | ⚖️ Optional — vendor pocketfft if our own RFFT is too slow |
| **FFT** | KissFFT | Mark Borgerding | BSD-3-Clause (license-clean!) | Low | Slightly slower than FFTW | Very low | N/A | ✅ Alternative — vendor as fallback if our own RFFT has bugs |
| **Onset detection** | Spectral flux | Bello 2005 | Public domain algorithm | Low (50 LOC) | Trivial (vectorised by compiler) | Negligible | ~85% onset F-measure on electronic | ✅ Adopt |
| **Onset detection** | Complex-domain flux | Bello 2003 | Public domain | Medium | 2× spectral flux | Negligible | Marginally higher | ⛔ Skip for v1.0; revisit if accuracy is poor |
| **Onset detection** | HFC (high-frequency content) | Masuda 2003 / Bello 2005 | Public domain | Low | Same as spectral flux | Negligible | Better on percussion | ✅ Optional weight — apply HFC weights to spectral flux frames |
| **Tempo** | Autocorrelation of onset envelope | Davies 2007 | Public domain | Low (100 LOC) | Negligible | ~200 KB for autocorr buffer | ~80% on Extended Ballroom | ✅ Adopt as primary |
| **Tempo** | Comb-filter bank | Davies 2007 / aubio | Public domain algorithm; GPL implementation | Medium (300 LOC) | Higher (N comb filters) | ~500 KB | ~83% on Extended Ballroom | ✅ Adopt as alternative — expose via Params |
| **Tempo** | Ellis DP beat tracker | Ellis 2007 | Public domain algorithm | Medium (250 LOC) | Slightly higher than autocorr | ~1 MB for DP cost matrix | ~85% on Extended Ballroom | ✅ Adopt as refinement stage — gives beat positions, not just tempo |
| **Tempo** | Multi-hypothesis tempo tracking | Essentia TempoTapMax | AGPL code; algorithm partly published | High | Higher | Higher | ~87% (with ML tuning) | ⛔ Skip — too complex for handheld |
| **Chroma** | Naive PCP (sum bin magnitudes by pitch class) | Fujishima 1999 | Public domain | Low (80 LOC) | Negligible | ~4 KB / frame | Lower than HPCP | ⚠️ Use as fallback only |
| **Chroma** | HPCP (Harmonic Pitch Class Profile) | Gómez 2006 | Public domain algorithm | Medium (300 LOC) | Slightly higher (peak picking per frame) | ~4 KB / frame | Good baseline for key | ✅ Adopt as primary chroma |
| **Chroma** | CLP (chroma log pitch) | Müller & Ewert 2010 | Public domain | Medium | Comparable | Comparable | Slightly higher with proper tuning | ⏸️ Defer to v1.x |
| **Chroma** | NNLS chroma (non-negative least squares) | Mauch & Dixon 2010 | Public domain algorithm | High (NNLS solver) | High | Higher | Higher | ⛔ Skip — too complex for v1.0 |
| **Key** | Krumhansl–Schmuckler with original profiles | Krumhansl 1990 | Public domain | Very low (50 LOC) | Negligible | Negligible | ~50% on classical, lower on modern | ⚠️ Use only as a baseline |
| **Key** | Krumhansl–Schmuckler with Temperley profiles | Temperley 1999 | Public domain | Very low | Negligible | Negligible | ~55% on mixed, ~60% on EDM | ✅ Adopt as default |
| **Key** | Temperley's Bayes-style key-finding | Temperley 2002 | Public domain algorithm | High | Higher | Higher | ~62% on classical | ⛔ Skip — too complex; we don't need the extra 2% |
| **Key** | Key templates from audio (Náth / MIREX 2013 winner) | Various | Public domain algorithms | Medium | Comparable | Comparable | Comparable to Temperley | ✅ Consider for v1.x tuning |
| **Key (data-driven)** | Convolutional neural network | Various 2018– | Various | Very high | Very high on CPU, NN accelerators rare on handheld | 5–50 MB model weights | Highest (90%+) | ⛔ Explicitly excluded — no ML per user constraint |

### 5.1 Algorithm selection rationale

**FFT:** Implement a small radix-2 RFFT in-house. The algorithm is public domain, the code is ~200 lines, and we avoid any dependency. If our implementation has correctness or performance issues, fall back to vendoring `kiss_fft` (BSD-3-Clause, license-clean, ~600 LOC, no external deps). FFTW3 is rejected because its default build is GPL; only the `pocketfft` standalone module is BSD-3-Clause and is a viable fallback.

**Onset:** Spectral flux with HFC weighting. This is the simplest robust choice, supported by Bello's 2005 tutorial showing it has the best cost/performance ratio for music with strong transients (which covers our target genre: electronic / tracker music). The HFC weighting emphasises high-frequency content (drums, hi-hats) which is where electronic tracks put the strongest tempo cues.

**Tempo:** Two-stage. (1) Autocorrelation of the onset envelope — gives a global tempo estimate robust to small timing errors. (2) Ellis 2007 DP beat tracker as a refinement step — produces a beat sequence and a confidence estimate; we use it to validate the autocorrelation result and to break ties when the autocorrelation has multiple equally-strong peaks (the classic half/double-tempo ambiguity). Both algorithms are public-domain and well-documented.

**Chroma:** HPCP (Gómez 2006). Naive PCP is too noisy for polyphonic audio; HPCP only counts peaks (not all bins) which dramatically reduces noise from non-tonal content. The peak-picking adds ~50% CPU over PCP but the resulting key accuracy improvement (per Gómez's published numbers, +10–15% over naive PCP) is worth it.

**Key:** Krumhansl–Schmuckler with Temperley profiles. The algorithm is unchanged from the 1990s; only the reference profiles have been improved since. We will use Temperley's 1999 profiles by default and expose the original Krumhansl profiles as a config option for comparison and tuning.

---

## 6. Library Architecture

### 6.1 Module layout

The library is divided into six modules. Each module has a public header (in `include/taktful/`) and a private implementation (in `src/`). Modules depend only on modules below them in the list — no circular dependencies.

```
taktful::wav_io        WAV decoding + sample format conversion
taktful::dsp           FFT, windowing, magnitude/phase, log-frequency mapping
taktful::onset         Spectral flux, peak picking, onset envelope
taktful::tempo         Autocorrelation + Ellis DP, BPM + confidence
taktful::chroma        HPCP chroma extraction
taktful::key           HPCP aggregation + Krumhansl–Schmuckler scoring
taktful::api           Top-level analyze() entry point, params, results structs
```

### 6.2 Directory structure (repo)

```
taktful/
├── CMakeLists.txt
├── Makefile                      # standalone Makefile fallback (no CMake needed)
├── README.md
├── LICENSE                       # MIT
├── include/
│   └── taktful/
│       ├── taktful.h                # umbrella include
│       ├── api.h                 # analyze(), Params, Result
│       ├── wav_io.h
│       ├── dsp.h
│       ├── onset.h
│       ├── tempo.h
│       ├── chroma.h
│       ├── key.h
│       └── version.h
├── src/
│   ├── wav_io.cpp
│   ├── dsp.cpp
│   ├── fft.cpp                   # in-house radix-2 RFFT
│   ├── onset.cpp
│   ├── tempo.cpp
│   ├── chroma.cpp
│   ├── key.cpp
│   └── api.cpp
├── cli/
│   ├── analyze_wav.cpp
│   ├── bpm_probe.cpp
│   └── key_probe.cpp
├── third_party/
│   └── kiss_fft/                 # vendored BSD-3 FFT, only built if TAKTFUL_USE_KISSFFT=ON
├── tests/
│   ├── unit/
│   └── golden/                   # WAV + expected JSON pairs
├── data/
│   └── ref_profiles.h            # Krumhansl + Temperley baked-in profiles
└── docs/
    └── dev_plan.md               # this file
```

### 6.3 Public API sketch

The API is intentionally minimal: one entry point for the high-level "give me everything" use case, plus lower-level entry points for tracker UIs that want to visualise intermediate stages.

```cpp
// include/taktful/api.h

#include <cstdint>
#include <vector>
#include <memory>
#include <taktful/version.h>

namespace taktful {

// ---- Input ----

struct WavData {
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    std::vector<float> samples;   // mono mixdown (interleaved → averaged), float [-1, 1]
                                 // length = num_frames (post-mixdown)
};

enum class WavError {
    OK = 0,
    FileNotFound,
    NotAWav,
    UnsupportedFormat,           // compressed WAV, exotic chunks, etc.
    UnsupportedBitDepth,
    OutOfMemory,
    Truncated,
};

WavError load_wav(const char* path, WavData& out);

// ---- Tunables ----

struct Params {
    // DSP
    uint32_t fft_size = 4096;       // power of two, 256..16384
    uint32_t hop_size = 2048;       // must be <= fft_size
    uint32_t target_sample_rate = 11025;  // internal downsample (decimate)
                                          // 11025 is enough for key (low end of piano)
                                          // and reduces CPU by 4× vs 44100

    // Onset
    enum class OnsetMethod {
        SpectralFlux,
        SpectralFluxHFC,           // weighted by frequency bin (default)
    };
    OnsetMethod onset_method = OnsetMethod::SpectralFluxHFC;

    // Tempo
    float tempo_min_bpm = 60.0f;
    float tempo_max_bpm = 200.0f;
    bool enable_dp_refinement = true;   // run Ellis DP after autocorrelation

    // Chroma
    float chroma_ref_freq = 440.0f;      // A4 reference
    bool  use_hpcp = true;                // false → naive PCP
    bool  apply_pre_filter = true;        // spectral whitening before chroma

    // Key
    enum class KeyProfiles {
        KrumhanslOriginal,
        Temperley1999,                 // default
        Temperley2007,
    };
    KeyProfiles key_profiles = KeyProfiles::Temperley1999;
    bool return_alternatives = true;       // populate Result::key_alternatives
    uint8_t num_alternatives = 5;
};

// ---- Output ----

enum class Key {
    CMaj, CsMaj, DMaj, DsMaj, EMaj, FMaj, FsMaj, GMaj, GsMaj, AMaj, AsMaj, BMaj,
    CMin, CsMin, DMin, DsMin, EMin, FMin, FsMin, GMin, GsMin, AMin, AsMin, BMin,
    Unknown,
};

const char* to_string(Key);

struct KeyResult {
    Key key = Key::Unknown;
    float confidence = 0.0f;          // 0..1, cosine similarity of best match
    struct Alternative {
        Key key;
        float confidence;
    };
    std::vector<Alternative> alternatives;   // sorted desc, size = Params::num_alternatives
};

struct TempoResult {
    float bpm = 0.0f;
    float confidence = 0.0f;          // 0..1
    std::vector<float> beat_times_sec;   // optional, empty if DP disabled
};

struct AnalysisResult {
    TempoResult tempo;
    KeyResult key;
};

// ---- Entry points ----

AnalysisResult analyze(const WavData& audio, const Params& params = {});

// Low-level (for tracker UIs that want to visualise intermediate stages)
struct IntermediateFeatures {
    std::vector<float> onset_envelope;       // one per hop
    std::vector<std::vector<float>> chroma;  // [hop][12]
};

IntermediateFeatures compute_features(const WavData& audio, const Params& params = {});

} // namespace taktful
```

### 6.4 Data flow

```
                ┌───────────────────────────────────┐
                │           WAV file                 │
                └────────────────┬───────────────────┘
                                 │ load_wav()
                                 ▼
                ┌───────────────────────────────────┐
                │   mono mixdown + int→float        │
                │   + downsample to 11025 Hz        │
                └────────────────┬───────────────────┘
                                 │
                ┌────────────────┴──────────────────┐
                ▼                                     ▼
       ┌──────────────────┐                ┌──────────────────┐
       │  STFT (FFT)      │                │   (shared FFT)   │
       │  4096 / 2048     │                │                  │
       └────────┬─────────┘                └──────────────────┘
                │
       ┌────────┴─────────┐
       ▼                    ▼
┌──────────────┐    ┌──────────────────┐
│ spectral flux│    │   HPCP chroma    │
│ + HFC weight │    │   per frame     │
└──────┬───────┘    └────────┬─────────┘
       │                     │
       ▼                     ▼
┌──────────────┐    ┌──────────────────┐
│ peak pick    │    │ aggregate PCP    │
│ → onset env  │    │ across song      │
└──────┬───────┘    └────────┬─────────┘
       │                     │
       ▼                     ▼
┌──────────────┐    ┌──────────────────┐
│ autocorrel.  │    │ correlate vs 24   │
│ + Ellis DP   │    │ Temperley profiles│
│ → BPM + conf │    │ → key + ranked    │
└──────┬───────┘    └────────┬─────────┘
       │                     │
       └──────────┬──────────┘
                  ▼
        ┌──────────────────┐
        │ AnalysisResult   │
        │  .tempo          │
        │  .key            │
        └──────────────────┘
```

### 6.5 Threading and allocation policy

- **Threading:** Single-threaded by default. The pipeline is CPU-bound for ≤ 4-minute audio (a few hundred ms on a Cortex-A7) and threading adds complexity the tracker doesn't need. The user can run multiple `analyze()` calls in parallel if they need to.
- **Allocation:** All heap allocations happen inside `analyze()`; intermediate buffers are reused across stages. We never call `new` / `malloc` inside an inner loop. The library's working set (excluding the input audio buffer) is bounded: one FFT buffer (≤ 64 KB), one onset envelope (≤ 80 KB for a 4-minute song at hop=2048 / 11025 Hz), one chroma matrix (≤ 1.5 MB for a 4-minute song × 12 floats per hop).
- **Exceptions:** The library never throws. All error paths return error codes or fill in `Result::key = Key::Unknown`.

---

## 7. Implementation Plan — Per Module

### 7.1 WAV I/O (`wav_io.cpp`)

**Scope:** Parse RIFF / WAV container, decode PCM (8/16/24/32-bit integer) and IEEE 754 32-bit float samples, perform channel mixdown to mono, return float samples in `[-1, 1]`.

**Algorithm:** Standard RIFF parser. Read 12-byte header (RIFF, size, WAVE). Iterate chunks: `fmt ` (decode WAVEFORMATEX — sample rate, channels, bits, format tag), `data` (sample payload). Skip unknown chunks (`LIST`, `fact`, etc.). Convert samples to float, average channels to mono. No resampling here — resampling happens in the next stage if needed.

**Edge cases to handle:**
- WAVE_FORMAT_EXTENSIBLE (tag 0xFFFE) with valid bits ≠ container bits.
- 24-bit samples (no native type; load 3 bytes at a time and sign-extend).
- Float samples (WAVE_FORMAT_IEEE_FLOAT, tag 3).
- Truncated `data` chunk (length field lies; trust the file size).
- RF64 (large-file WAV variant) — defer to v1.x, fail gracefully in v1.0.

**Complexity:** ~250 lines of code. No external dependencies.

**Tests:** Unit tests against synthetic WAVs generated by Python's `wave` module at every supported bit depth / channel count. Golden test against a real-world WAV from the test corpus.

### 7.2 DSP primitives (`dsp.cpp`, `fft.cpp`)

**Scope:** Windowing (Hann, Hamming, Blackman–Harris), in-house radix-2 RFFT, magnitude spectrum, log-frequency spectrum mapping (used by chroma).

**FFT algorithm:** Iterative radix-2 Cooley–Tukey with bit-reversal permutation. Real input → complex output of length N/2+1. Pre-computed twiddle factors (sine / cosine table) cached per FFT size. Supports sizes 256, 512, 1024, 2048, 4096, 8192, 16384.

**Windowing:** Apply window in-place to the FFT input buffer. Default is Hann. Blackman–Harris is also supported for chroma (slightly better side-lobe suppression).

**Mag / phase:** `mag = sqrt(re² + im²)`. Phase is computed lazily (only needed by complex-domain onset, which is not in v1.0).

**Log-frequency mapping:** For each bin $k$ of the magnitude spectrum, compute the corresponding frequency $f_k = k \cdot f_s / N$, then map to a fractional MIDI note $m = 69 + 12 \log_2(f_k / 440)$. Bin $k$ contributes to chroma bin `(int)round(m) mod 12` weighted by its magnitude. This is the foundation of both naive PCP and HPCP.

**Complexity:** ~250 lines total. No external dependencies.

**Performance target:** On a 1 GHz Cortex-A7, a 4096-point RFFT should take ≤ 300 µs. With 4-minute audio at 11025 Hz / hop 2048, we have ~1300 FFTs to do → ~400 ms of FFT work. Acceptable.

**Fallback:** If our FFT has correctness issues, vendor `kiss_fft` (BSD-3-Clause). The build system supports `TAKTFUL_FFT=kissfft` to swap implementations.

### 7.3 Onset detection (`onset.cpp`)

**Scope:** Compute spectral flux with optional HFC weighting, smooth, pick peaks, produce onset envelope (one value per hop).

**Algorithm:**

1. For each STFT frame $t$, compute magnitude spectrum $|X_t(k)|$.
2. Convert to dB: $m_t(k) = 20 \log_{10}(|X_t(k)| + \epsilon)$ where $\epsilon = 10^{-6}$.
3. Spectral flux: $SF(t) = \sum_k \max(0, m_t(k) - m_{t-1}(k)) \cdot w(k)$ where $w(k)$ is the HFC weight $w(k) = k / N$ (linear in frequency). HFC weighting is disabled by `Params::onset_method = SpectralFlux`.
4. Smooth $SF$ with a 3-tap moving average.
5. (Peak picking is deferred to the tempo module — the onset *envelope* is the output here, not the peak list.)

**Complexity:** ~120 lines. Negligible CPU.

**Tests:** Unit tests on synthetic clicks (impulse train) at known BPMs; verify the onset envelope peaks at the click times.

### 7.4 Tempo (`tempo.cpp`)

**Scope:** Autocorrelation-based tempo estimation with optional Ellis 2007 DP refinement.

**Algorithm (autocorrelation):**

1. Resample the onset envelope to a fine grid (e.g. 100 Hz — i.e. one value every 10 ms) via linear interpolation. Length for a 4-minute song ≈ 2400 samples.
2. Compute autocorrelation $R(\tau) = \sum_t e(t) \cdot e(t+\tau)$ for $\tau$ in the range corresponding to `tempo_min_bpm .. tempo_max_bpm` (e.g. 60–200 BPM at 100 Hz → $\tau$ from 30 to 100).
3. Find peaks in $R(\tau)$. The strongest peak's $\tau$ gives the beat period: $BPM = 60 / (\tau \cdot dt)$ where $dt = 10 \text{ ms}$.
4. **Octave correction:** If the second-strongest peak is at $\tau/2$ or $2\tau$ and is within 80% of the strongest, prefer the slower tempo (electronic music typically has a "true" tempo below 130 BPM; users prefer underestimates to overestimates for slow genres).
5. **Confidence:** $c = R(\tau_{\text{best}}) / R(0)$, normalised. Range 0..1.

**Algorithm (Ellis DP refinement, optional):**

1. Use the autocorrelation BPM as the global tempo $\tau$.
2. Compute an onset strength signal (the onset envelope, normalised).
3. Build a DP score function $C(t) = O(t) + \alpha \sum_{t'} C(t') \exp(-(t - t' - \tau)^2 / 2\sigma^2)$ where $O(t)$ is onset strength and $\alpha$ controls how strictly the tempo is enforced (Ellis uses $\alpha = 100$, $\sigma = 0.5 \tau$).
4. Backtrack to recover beat times.
5. The inter-beat intervals give a refined BPM estimate; confidence is the variance of inter-beat intervals (low variance = high confidence).

**Complexity:** ~300 lines total. CPU is dominated by the autocorrelation, which is O(N²) but with small N (~2400 samples × 70 lag values ≈ 170K multiplies).

**Tests:** Synthetic click tracks at 60, 90, 120, 140, 160, 180 BPM. Real-world tests against the Extended Ballroom dataset (see §10).

### 7.5 Chroma (`chroma.cpp`)

**Scope:** Per-frame HPCP chroma (12-dim vector per STFT frame), aggregated across the song into a single PCP used by the key module.

**Algorithm (HPCP, per Gómez 2006):**

1. For each STFT frame, compute magnitude spectrum.
2. Apply spectral whitening (optional, `Params::apply_pre_filter`): divide each bin by the local mean over a 1/3-octave window. This makes the spectrum more "flat" and reduces timbre-dependent key errors.
3. Find local peaks in the magnitude spectrum (a bin is a peak if it's larger than its 2 neighbours on each side and above a small absolute threshold).
4. For each peak at frequency $f$, compute the corresponding pitch class $p = \text{round}(12 \log_2(f / f_{\text{ref}})) \mod 12$ where $f_{\text{ref}} = $ `Params::chroma_ref_freq`.
5. Add the peak magnitude to `chroma[p]`.
6. Normalise the 12-dim chroma vector (L2 norm).
7. Optionally smooth across frames with a 5-frame median filter.

**Algorithm (aggregation):** Across all frames, take the L1-mean of the per-frame chroma vectors. This is the song-level PCP. (Alternative: L2-mean, which emphasises consistently-strong pitch classes; we default to L1 which is what Gómez's original paper uses.)

**Complexity:** ~250 lines. CPU is dominated by peak picking (linear scan over ~2048 bins per frame × ~1300 frames = 2.7M operations).

**Tests:** Synthetic sinusoids at known frequencies; verify the chroma vector peaks at the expected pitch class. Real-world tests against the MTG Key dataset (see §10).

### 7.6 Key (`key.cpp`)

**Scope:** Given the song-level PCP, return the best-matching key plus ranked alternatives.

**Algorithm (Krumhansl–Schmuckler with Temperley profiles):**

1. Take the 12-dim PCP vector $v$ from the chroma module.
2. Normalise $v$ to zero mean (subtract $\bar v$ from each element).
3. For each of the 24 reference profiles $p_k$ (12 major + 12 minor), compute the Pearson correlation $r_k = \text{corr}(v, p_k)$.
4. Sort by $r_k$ descending. Top match → `Result::key`. Top N (default 5) → `alternatives`.
5. **Confidence:** $c = (r_1 - r_2) / 2 + 0.5$, mapped to [0, 1]. If the top match is much better than the runner-up, confidence is high.

**Reference profiles:** Both Krumhansl's original 1982 profiles and Temperley's 1999 profiles are baked into `data/ref_profiles.h` as `static const float[]` arrays. The choice is made via `Params::key_profiles`.

**Complexity:** ~120 lines. CPU negligible (24 correlations × 12 multiplies = 288 multiplies).

**Tests:** Synthetic pure-sine tests at every pitch class → should detect the corresponding major and relative minor keys. Real-world tests against the MTG Key dataset.

### 7.7 Top-level API (`api.cpp`)

**Scope:** Glue the modules together. Manage the shared FFT plan, intermediate buffers, and error propagation.

**Algorithm:** Straight-line sequence of calls into the lower modules. No surprises. The single function `analyze()` is the main entry point; `compute_features()` is the lower-level entry point that returns intermediate features for UI visualisation.

**Complexity:** ~100 lines.

---

## 8. Memory & Performance Budget

The following table gives the budget for a "worst case" target file: a 4-minute stereo 16-bit 44.1 kHz WAV (about 40 MB on disk; ~21 million samples after decode).

| Resource | Budget | Notes |
|---|---|---|
| **Static code size** (libtaktful.a, stripped) | ≤ 200 KB | Goal, not hard limit. Excludes the C++ runtime. |
| **Static code size** (per-CLI binary, statically linked against libstdc++) | ≤ 800 KB | Includes C++ runtime + libtaktful. |
| **Heap peak** (excluding WavData) | ≤ 2 MB | FFT buffer + onset envelope + chroma matrix. |
| **Stack usage** | ≤ 8 KB | No deep recursion. FFT is iterative. |
| **Audio buffer** | ≤ 21 MB (44.1 kHz) / 5 MB (after downsample to 11025 Hz) | Downsample happens in `load_wav` or in `analyze` first step. |
| **CPU time on Cortex-A7 @ 1 GHz** | ≤ 1.5 s for 4-minute audio | FFT is ~400 ms; onset ~50 ms; tempo ~10 ms; chroma ~400 ms; key ~1 ms. |
| **CPU time on x86_64 @ 3 GHz** | ≤ 250 ms | For development / CLI testing. |

**Allocation policy:** All large buffers are `std::vector<float>` allocated once at the start of `analyze()` and reused. No `new`/`malloc` in inner loops. No exceptions in inner loops. No RTTI required (we don't use `dynamic_cast`).

**Memory profiling:** The CLI tool `analyze_wav --stats` will print the peak heap usage (via `mallinfo2` on Linux, or a custom allocator wrapper) so we can verify the budget on the target device.

---

## 9. CLI Tools Specification

Three CLI tools ship alongside the library. Each is a single `.cpp` file in `cli/`, built as a standalone binary by CMake (and by the Makefile fallback).

### 9.1 `analyze_wav`

The flagship tool. Loads a WAV, runs the full analysis, prints results.

```
Usage:
  analyze_wav [options] <path-to-wav>

Options:
  --json                  Output JSON (default). Human-readable alternative: --text.
  --text                  Output human-readable text.
  --params <file.json>    Override defaults from a JSON params file.
  --fft-size <N>          FFT size (256..16384, power of two). Default 4096.
  --hop-size <N>          Hop size. Default 2048.
  --no-dp                 Disable Ellis DP refinement.
  --profiles <name>       Key profiles: krumhansl | temperley1999 | temperley2007
  --alternatives <N>      Number of alternative keys to print. Default 5.
  --stats                 Print memory + CPU stats at the end.
  --verbose               Print every intermediate stage to stderr.
```

**JSON output schema:**
```json
{
  "file": "track.wav",
  "duration_sec": 240.0,
  "sample_rate": 11025,
  "tempo": {
    "bpm": 128.0,
    "confidence": 0.82,
    "beat_times_sec": [0.469, 0.938, 1.407, ...]
  },
  "key": {
    "key": "A minor",
    "confidence": 0.74,
    "alternatives": [
      {"key": "A minor",       "confidence": 0.74},
      {"key": "C major",       "confidence": 0.69},
      {"key": "E minor",       "confidence": 0.55},
      {"key": "G major",       "confidence": 0.48},
      {"key": "D minor",       "confidence": 0.42}
    ]
  },
  "stats": {
    "peak_heap_kb": 1820,
    "cpu_ms_total": 412,
    "cpu_ms_fft": 387,
    "cpu_ms_chroma": 21,
    "cpu_ms_key": 0
  }
}
```

### 9.2 `bpm_probe`

Lighter tool that only does tempo. Useful for quick tuning.

```
Usage:
  bpm_probe [options] <path-to-wav>

Options:
  --min <bpm>             Minimum tempo. Default 60.
  --max <bpm>             Maximum tempo. Default 200.
  --no-dp                 Disable DP refinement.
  --text                  Output text (default).
  --onset-csv <file>      Dump onset envelope to CSV for plotting.
  --acorr-csv <file>      Dump autocorrelation curve to CSV for plotting.
```

**Text output:**
```
BPM: 128.0 (confidence: 0.82, method: autocorrelation + DP)
```

### 9.3 `key_probe`

Lighter tool that only does key. Useful for quick tuning.

```
Usage:
  key_probe [options] <path-to-wav>

Options:
  --profiles <name>       Key profiles: krumhansl | temperley1999 | temperley2007
  --ref-freq <Hz>         A4 reference. Default 440.
  --naive-pcp             Use naive PCP instead of HPCP.
  --chroma-csv <file>     Dump song-level PCP (12-dim) to CSV.
  --per-frame-csv <file>  Dump per-frame chroma matrix to CSV.
```

**Text output:**
```
Key: A minor (confidence: 0.74)
Alternatives:
  1. A minor       0.74
  2. C major       0.69
  3. E minor       0.55
  4. G major       0.48
  5. D minor       0.42
```

### 9.4 CSV format conventions

All `--*-csv` outputs are RFC-4180 CSV with a header row. The first column is always `frame_index` (0-based). Numeric values are printed with 6 decimal places. This makes them directly plottable by `gnuplot`, `matplotlib`, or by pasting into a spreadsheet.

### 9.5 Exit codes

All three tools share the same exit code convention:
- `0` — success, analysis complete.
- `1` — file not found.
- `2` — file is not a valid WAV (parse error).
- `3` — unsupported WAV format (e.g. compressed WAV).
- `4` — out of memory.
- `5` — analysis failed (e.g. silent file → no tempo).
- `64` — bad command-line arguments.

---

## 10. Test & Validation Strategy

Testing happens at four levels: (1) unit tests on each module, (2) integration tests on the full pipeline, (3) golden tests on a curated corpus, and (4) accuracy benchmarks against published ground-truth datasets.

### 10.1 Unit tests (`tests/unit/`)

Each module has its own unit test file, built with a tiny header-only test framework (we'll vendor `utest.h` from Jeff Muizelaar — public domain, single file, no deps). Target: ≥ 80% line coverage on each module.

Specific test cases per module:

**wav_io:**
- 8/16/24/32-bit integer, mono + stereo + 5.1 surround WAVs.
- 32-bit float WAV.
- WAVE_FORMAT_EXTENSIBLE with valid bits ≠ container bits.
- Truncated `data` chunk.
- Non-WAV file (e.g. MP3 with WAV extension) → returns `NotAWav`.
- Empty file → returns `Truncated`.

**dsp / fft:**
- FFT of a pure sinusoid → peaks at the expected bin.
- FFT of DC → only bin 0 is non-zero.
- FFT of impulse → flat spectrum.
- FFT of a known input compared against a numpy reference FFT (ε ≤ 1e-4 relative error).
- All supported FFT sizes (256..16384).

**onset:**
- Synthetic impulse train at 120 BPM → onset envelope peaks at exactly 0.5 s intervals.
- White noise → onset envelope is flat (no spurious peaks).
- Real WAV from corpus → peaks line up with hand-annotated onsets (within ±50 ms).

**tempo:**
- Synthetic impulse trains at 60, 80, 100, 120, 140, 160, 180, 200 BPM → returned BPM within 0.5 BPM.
- Half-tempo / double-tempo stress test: impulse train at 140 BPM should return 140, not 70 or 280.
- Silent file → returns `TempoResult.bpm = 0, confidence = 0`.

**chroma:**
- Pure sine at A4 (440 Hz) → chroma peaks at pitch class 9 (A).
- Pure sine at C5 (523 Hz) → chroma peaks at pitch class 0 (C).
- Pure sines at every pitch class in turn.
- Sine at slightly off-tune A4 (445 Hz) → still peaks at A (pitch tolerance).

**key:**
- Synthetic C-major triad (C4, E4, G4) → returns C major.
- Synthetic A-minor triad (A3, C4, E4) → returns A minor.
- All 12 major triads → all 12 major keys.
- All 12 minor triads → all 12 minor keys.

### 10.2 Integration tests (`tests/integration/`)

End-to-end tests on synthetic and real audio. Target: 100% pass rate on synthetic, ≥ 80% on real.

- Load WAV → analyze → check that `tempo.bpm > 0` and `key != Key::Unknown`.
- Run on a 5-second silent file → result is `Key::Unknown, bpm = 0`.
- Run on a 1-second file → result is `Key::Unknown` (too short for key), `bpm` may still be valid.
- Run on a 10-minute file → no crash, completes within 5 s on x86_64.

### 10.3 Golden tests (`tests/golden/`)

A curated corpus of WAV files with hand-verified or community-verified ground truth, stored in `tests/golden/corpus/`. Each WAV is paired with a `.expected.json` file. The integration test runner compares the actual JSON output of `analyze_wav --json` against the expected JSON, with a small tolerance on tempo (±0.5 BPM) and confidence (±0.05).

**Corpus contents (target ~30 files):**
- 5 synthetic click tracks at known BPMs.
- 5 simple melodies with known keys (C major, A minor, F major, etc.).
- 10 electronic / tracker tracks from public-domain sources (e.g. modules from modarchive.org rendered to WAV via Schism Tracker). These cover the user's target genre.
- 10 tracks from the MIREX / MTG Key datasets (with proper attribution; the datasets themselves are not redistributed — only the WAV paths are referenced).

### 10.4 Accuracy benchmarks

We run the full pipeline against two published ground-truth datasets and compare our results to published numbers. The CLI tool `run_benchmark` automates this.

**Tempo: Extended Ballroom dataset.**
- Source: http://mtg.upf.edu/ismir2004/contest/tempoContest/node5.html
- Size: 698 tracks across 8 ballroom dance genres, with annotated BPM.
- Metric: % of tracks where the estimated BPM is within ±4% of ground truth (this is the MIREX 2004–2018 metric).
- Target: ≥ 80% accuracy. (Published autocorrelation methods achieve ~85%; TempoCNN ML method achieves ~92% but is excluded by our no-ML constraint.)

**Key: MTG Key dataset.**
- Source: https://zenodo.org/record/1095691
- Size: 1,159 EDM tracks, with annotated key (Camelot + traditional notation).
- Metric: % of tracks where the estimated key matches ground truth. We will also report "correct or relative major/minor" accuracy (e.g. A minor estimate vs C major truth — these are musically interchangeable).
- Target: ≥ 60% strict accuracy, ≥ 75% with relative-key tolerance. (Published non-ML methods achieve ~60–65% strict on this dataset; CNN-based methods achieve ~75–80% but are excluded by our no-ML constraint.)

**Key: GTZAN genre dataset (key annotations by Wei Chih & Yang 2012).**
- Source: http://marsyas.info/downloads/datasets.html
- Size: 1,000 tracks, 30 s each, 10 genres.
- Metric: same as MTG Key.
- Target: ≥ 55% strict accuracy (we expect lower than EDM because GTZAN is mixed genre).

### 10.5 Continuous integration

- GitHub Actions (or equivalent) on push.
- Build matrix: GCC 9 / Clang 12 / GCC 11 on Ubuntu 22.04, plus a native ARM build on a QEMU-emulated Cortex-A7.
- Run unit + integration tests on every push.
- Run accuracy benchmarks on every release tag (not on every push — they take ~30 min).

---

## 11. Roadmap — Phased Plan

The roadmap is split into four milestones. Each milestone is shippable: it produces a binary that does something useful, has tests, and can be integrated into the tracker.

### 11.1 Milestone M0 — Project bootstrap (0.5 week)

**Goal:** Empty repo, builds cleanly on all target platforms, CI green.

**Deliverables:**
- Repo skeleton with the directory structure from §6.2.
- CMakeLists.txt that builds an empty `libtaktful` static lib.
- Standalone Makefile that does the same.
- `LICENSE` file (MIT).
- `README.md` with build instructions.
- GitHub Actions CI: build on Ubuntu / macOS / Windows, plus a QEMU-ARM build.
- `utest.h` vendored.

**Exit criteria:** `cmake --build . && ctest` passes (with zero tests, just to verify the harness).

### 11.2 Milestone M1 — MVP: WAV load + tempo (1.5 weeks)

**Goal:** `bpm_probe <wav>` returns a correct BPM for synthetic click tracks and a reasonable estimate for electronic music.

**Deliverables:**
- WAV I/O (§7.1) — full PCM support, mono mixdown, no resampling yet.
- DSP / FFT (§7.2) — radix-2 RFFT, Hann window, magnitude spectrum.
- Onset (§7.3) — spectral flux with HFC weighting.
- Tempo (§7.4) — autocorrelation only (no DP yet).
- CLI: `bpm_probe` with `--text` and `--onset-csv` flags.
- Unit tests for all of the above.
- One golden test: 4 synthetic click tracks (60, 90, 120, 160 BPM).

**Exit criteria:** `bpm_probe synthetic_120.wav` returns `120.0 ± 0.5 BPM`. On a 4-minute electronic track, returns a BPM within ±4% of the value reported by `aubiotrack` (the aubio CLI tool — used as an external reference, not as a dependency).

**Risk:** FFT correctness. Mitigation: cross-check against numpy `np.fft.rfft` on the same input.

### 11.3 Milestone M2 — Chroma + key (1.5 weeks)

**Goal:** `key_probe <wav>` returns the correct key for synthetic triads and a reasonable estimate for real music.

**Deliverables:**
- Resampling in `load_wav` or as a separate step — linear interpolation downsample to 11025 Hz.
- Chroma (§7.5) — HPCP, peak picking, aggregation, spectral whitening.
- Key (§7.6) — Krumhansl + Temperley profiles, cosine correlation, ranked alternatives.
- CLI: `key_probe` with `--text` and `--chroma-csv` flags.
- Unit tests for chroma and key.
- Golden tests: 12 major triads, 12 minor triads, plus 3 real-world tracks with verified keys.

**Exit criteria:** All 24 synthetic triad tests pass. On a real electronic track, the correct key is in the top 3 alternatives ≥ 80% of the time.

**Risk:** HPCP peak picking is sensitive to threshold tuning. Mitigation: expose thresholds as `Params` fields; tune against the MTG Key dataset subset (100 tracks) before locking defaults.

### 11.4 Milestone M3 — v1.0: full pipeline + validation (1.5 weeks)

**Goal:** Ship v1.0.0. `analyze_wav` returns both key and tempo, with confidence scores. Accuracy targets met on the benchmark datasets.

**Deliverables:**
- Top-level `analyze()` API (§7.7).
- CLI: `analyze_wav` with `--json` output.
- Ellis DP beat tracker (§7.4, second half) — optional, gated behind `Params::enable_dp_refinement`.
- Memory + CPU stats in `analyze_wav --stats`.
- Accuracy benchmarks: Extended Ballroom (tempo), MTG Key (key).
- Documentation: `docs/api.md`, `docs/tuning.md`, updated `README.md`.
- v1.0.0 git tag.

**Exit criteria:**
- Extended Ballroom tempo accuracy ≥ 80%.
- MTG Key strict accuracy ≥ 60%, relative-key accuracy ≥ 75%.
- Peak heap ≤ 2 MB on a 4-minute track on x86_64.
- CPU time ≤ 1.5 s on a 4-minute track on a QEMU-emulated Cortex-A7 @ 1 GHz.

**Risk:** Accuracy targets may not be met with default parameters. Mitigation: tune on a held-out subset of the benchmark datasets; if accuracy falls short, document the gap and ship with the best-tuned parameters anyway (the targets are aspirational, not blocking).

### 11.5 Milestone M4 — v1.x: hardening + optimisation (open-ended)

**Goal:** Make the library production-grade for the tracker.

**Deliverables (in priority order):**
- ARM NEON intrinsics for the FFT, magnitude spectrum, window multiply, and RMS (compile-time `__ARM_NEON`, optional `-DTaktful_ENABLE_NEON=OFF`).
- Streaming API: `analyze_streaming(StreamSource&, Params)` that processes chunked audio. Useful for live input.
- Additional chroma variants: CLP (chroma log pitch) as an alternative to HPCP.
- Additional key profiles: Temperley 2007, custom user-loadable profiles.
- WAV variant support: RF64, WAVEX.
- Optional FLAC decode via vendored `dr_flac` (public domain).
- Optional Ogg/Vorbis decode via vendored `stb_vorbis` (public domain).
- Fuzzing: libFuzzer on `load_wav` to harden the parser.
- C API wrapper (`taktful.h` with `extern "C"`) for binding from Rust / Python / Lua.

---

## 12. Licensing Audit Notes

This section documents the licensing constraints that govern the project, both for the user's reference and for any future contributor.

### 12.1 License of this library

The library is **MIT-licensed** (see `LICENSE` file). This means anyone can use, modify, distribute, and sublicense the code with minimal restriction (the copyright + permission notice must be preserved). It is compatible with inclusion in proprietary software, which is the user's tracker-app need.

### 12.2 License of dependencies

We **do not link** against any GPL or AGPL library. Our dependency tree is:

| Dependency | License | Status |
|---|---|---|
| C++ standard library (libstdc++) | GPL-3.0-with-GCC-runtime-exception | ✅ OK — the GCC runtime exception permits use in MIT-licensed binaries without contamination. |
| C standard library (glibc / musl) | LGPL-2.1 | ✅ OK — dynamically linked, no contamination. For static linking against musl, musl is MIT-licensed. |
| `utest.h` (test framework) | Public domain / Unlicense | ✅ OK — no restrictions. |
| `kiss_fft` (optional FFT fallback) | BSD-3-Clause | ✅ OK — must reproduce copyright notice in docs. |
| `dr_flac` (optional, v1.x) | MIT-0 | ✅ OK. |
| `stb_vorbis` (optional, v1.x) | Public domain / MIT | ✅ OK. |

**No** dependency on libKeyFinder, aubio, Essentia, TarsosDSP, FFTW, libsndfile, libvorbis (in its GPL form), or Boost. **No** dependency on any GPL / AGPL code, statically or dynamically linked.

### 12.3 Algorithms vs. code

**Algorithms are not copyrightable** (per US law — *Baker v. Selden* 1879 and subsequent cases; similar in EU under the Software Directive 2009/24/EC Article 9(2)). What is copyrightable is the *expression* of the algorithm in source code. We are free to reimplement any published algorithm — including those used by GPL-licensed libraries — provided we write our own code from the algorithm description in a published paper or textbook, and do not copy the GPL library's source.

**Clean-room rule (must be followed by all contributors):**
1. The reference material for any algorithm implementation is the published paper, not a library's source code.
2. If a contributor has previously read the source of libKeyFinder / aubio / Essentia / TarsosDSP / QM-Vamp-plugins, they must not work on the corresponding module of this library. They can still work on other modules.
3. We *may* run GPL tools (aubio CLI, libKeyFinder CLI, Sonic Visualiser with QM plugins) on test WAVs and compare their outputs to ours. This is "black-box" use and does not contaminate our code. We document the comparison in `tests/golden/README.md` so that future maintainers understand why certain WAVs have certain expected outputs.

### 12.4 Attribution

The `README.md` will include an "Acknowledgements" section that lists:
- The original authors of each algorithm we use (Cooley & Tukey for FFT, Krumhansl, Schmuckler, Temperley for key finding, Gómez for HPCP, Ellis for DP beat tracking, Bello for onset detection).
- The libraries we used as black-box references for accuracy comparison (libKeyFinder, aubio, Essentia).
- The datasets used for benchmarking (Extended Ballroom, MTG Key, GTZAN).

### 12.5 Contributor License Agreement

Not required for v1.0 (single-author project). If external contributors join, a CLA may be added to allow future relicensing.

---

## 13. Risks & Open Questions

### 13.1 Risks

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| In-house FFT has subtle numerical bugs that hurt chroma accuracy | Medium | High | Cross-check against `np.fft.rfft` on every commit. Fall back to `kiss_fft` if issues persist. |
| HPCP threshold tuning is fragile across genres | Medium | Medium | Expose thresholds as Params; ship defaults tuned for electronic music; document how to tune for other genres. |
| Octave errors in tempo (half/double BPM) | High | Medium | Ellis DP refinement should resolve most cases; document the heuristic for ambiguous cases. |
| Accuracy targets (80% tempo, 60% key) not met on benchmark datasets | Medium | High | Tune parameters on a held-out subset; if still short, ship with best-tuned parameters and document the gap. |
| Memory budget exceeded on long files (> 4 min) | Low | High | Downsample to 11025 Hz aggressively (already planned); if still over, add streaming API earlier (move from M4 to M3). |
| Compiler differences (GCC vs. Clang) on handheld toolchain | Low | Low | CI builds on both; standard C++14 only, no compiler-specific extensions. |
| Tempo accuracy on tracks with no clear beat (ambient, drone) | Medium | Low | Document that tempo is undefined for these; return `bpm = 0, confidence = 0`. |

### 13.2 Open questions

These are decisions that should be made during implementation, not now:

1. **Should we ship a "fast" mode that disables Ellis DP and skips spectral whitening, in exchange for ~10% accuracy loss?** Probably yes — add as a `Params::fast_mode` flag. The tracker UI may need to re-run analysis live on every loop iteration.

2. **Should we support Camelot wheel notation alongside traditional key names?** Probably yes — it's a one-line mapping. Add as `Result::camelot` field.

3. **Should the library be usable from C (not just C++)?** A C wrapper is in M4. If the tracker is in Rust or another C-bindable language, this becomes M3 priority.

4. **What's the right default for `target_sample_rate`?** 11025 Hz is enough for chroma (covers up to ~5 kHz, which is the top of piano range) and gives a 4× speedup over 44100 Hz. But for high-frequency percussion (hi-hats at 10 kHz+), this may lose some onset information. Validate empirically on the test corpus.

5. **Should we support per-section analysis (e.g. key for each 30-second segment)?** Out of scope for v1.0 — the API assumes whole-file analysis. But the underlying chroma matrix is per-frame, so a future `analyze_sections()` could slice and re-aggregate without recomputing the FFT.

6. **Should the Ellis DP refinement be enabled by default?** Yes, in M3 — its main cost is the DP cost matrix (~1 MB) and CPU (small). But the user can disable it for speed.

7. **What about non-standard tunings (e.g. A4 = 432 Hz)?** Supported via `Params::chroma_ref_freq`. Document in the README.

---

## 14. References

This section lists every paper and resource used as a reference for the algorithms in this library. Contributors should consult these (and not library source code) when implementing.

### 14.1 FFT and DSP fundamentals

- **Cooley, J. W., & Tukey, J. W. (1965).** "An algorithm for the machine calculation of complex Fourier series." *Mathematics of Computation*, 19(90), 297–301. — The original radix-2 FFT paper. Public domain.
- **Smith, J. O. (2007).** *Spectral Audio Signal Processing.* W3K Publishing. Online at https://ccrma.stanford.edu/~jos/sasp/ — Comprehensive DSP reference.
- **Frigo, M., & Johnson, S. G. (2005).** "The design and implementation of FFTW3." *Proceedings of the IEEE*, 93(2), 216–231. — Reference for FFT implementation strategies.

### 14.2 Onset detection

- **Bello, J. P., Daudet, L., Abdallah, S., Duxbury, C., Davies, M., & Sandler, M. B. (2005).** "A tutorial on onset detection in music signals." *IEEE Transactions on Speech and Audio Processing*, 13(5), 1035–1047. — The standard reference. We use the spectral flux + HFC variant described in §III.
- **Masuda, T., et al. (2003).** "High-frequency-content based onset detection." — Original description of HFC weighting.

### 14.3 Tempo and beat tracking

- **Ellis, D. P. W. (2007).** "Beat tracking by dynamic programming." *Journal of New Music Research*, 36(1), 51–60. — The DP beat tracker we use. Algorithm in §2 is fully specified; we re-implement directly from the paper.
- **Davies, M. E. P., & Plumbley, M. D. (2007).** "Context-dependent beat tracking of musical audio." *IEEE Transactions on Audio, Speech, and Language Processing*, 15(3), 1009–1020. — Reference for the autocorrelation-based tempo stage.
- **McKinney, M. F., & Moelants, D. (2007).** "Ambiguity in tempo perception: What draws listeners to different metrical levels?" *Music Perception*, 24(3), 155–166. — Reference for the octave-error problem and the "prefer slower tempo" heuristic.
- **Schreiber, H. (2020).** "Music Tempo Estimation: Are We Done Yet?" *Transactions of the International Society for Music Information Retrieval*, 3(1). — Survey of recent tempo-estimation accuracy; gives us realistic targets.

### 14.4 Chroma and key

- **Fujishima, T. (1999).** "Realtime chord recognition of musical sound: A system using Common Lisp Music." *ICMC Proceedings.* — Original PCP / chroma concept.
- **Gómez, E. (2006).** "Tonal description of music audio signals." PhD thesis, Universitat Pompeu Fabra. — The HPCP algorithm we adopt. Full algorithm description in Ch. 3.
- **Krumhansl, C. L. (1990).** *Cognitive foundations of musical pitch.* Oxford University Press. — The original key-finding algorithm and reference profiles.
- **Temperley, D. (1999).** "What's key for key? The Krumhansl–Schmuckler key-finding algorithm reconsidered." *Music Perception*, 17(1), 65–100. — The modified profiles we adopt by default.
- **Temperley, D. (2007).** *Music and Probability.* MIT Press. — Source of the 2007 alternative profiles.

### 14.5 Datasets and benchmarks

- **Gouyon, F., et al. (2006).** "An evaluation of tempo estimation algorithms in MIREX 2005." *ISMIR.* — The Extended Ballroom dataset (698 tracks, BPM-annotated).
- **Faraldo Llovera, Á. (2017).** "MTG Key dataset." Zenodo. https://zenodo.org/record/1095691 — EDM key-annotated dataset, 1,159 tracks.
- **Tzanetakis, G., & Cook, P. (2002).** "Musical genre classification of audio signals." *IEEE Transactions on Speech and Audio Processing*, 10(5), 293–302. — GTZAN dataset.
- **MIREX 2018.** Music Information Retrieval Evaluation eXchange. http://www.music-ir.org/mirex — Annual benchmark; we use their evaluation metrics.

### 14.6 Software references (for output comparison, NOT source reading)

- **libKeyFinder** — `github.com/ibsh/libKeyFinder`. GPL-3.0. Used as a black-box reference for key accuracy.
- **aubio** — `aubio.org`. GPL-3.0+. Used as a black-box reference for tempo accuracy.
- **Essentia** — `github.com/MTG/essentia`. AGPL-3.0. Used as a black-box reference for both.
- **TarsosDSP** — `github.com/JorenSix/TarsosDSP`. GPL-3.0. Java reference.
- **Vamp / QM Vamp Plugins** — `vamp-plugins.org` and `github.com/c4dm/qm-vamp-plugins`. SDK is X11/BSD-3 (clean); plugins are GPL.

---

## 15. Appendix A — Build instructions (preview)

The build system supports two paths: CMake (primary) and a hand-written Makefile (fallback for environments without CMake, common on older handheld toolchains).

### 15.1 CMake (primary)

```bash
git clone <repo> taktful
cd taktful
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DTaktful_BUILD_CLI=ON -DTaktful_BUILD_TESTS=ON
cmake --build . -j
ctest --output-on-failure
```

Options:
- `-DTaktful_BUILD_CLI=ON` — build the three CLI tools. Default OFF.
- `-DTaktful_BUILD_TESTS=ON` — build unit + integration tests. Default OFF.
- `-DTaktful_USE_KISSFFT=ON` — vendor `kiss_fft` instead of using our in-house FFT. Default OFF.
- `-DTaktful_BUILD_STATIC=ON` — produce `libtaktful.a` instead of `libtaktful.so`. Default ON (we want static for embedding).

### 15.2 Makefile (fallback)

```bash
git clone <repo> taktful
cd taktful
make -j lib      # builds libtaktful.a
make -j cli      # builds the three CLI binaries
make -j tests    # builds and runs tests
```

The Makefile is hand-written, ~100 lines, with no variables that the user must override. It uses `g++` by default; override with `make CXX=clang++`.

### 15.3 Integration into the tracker

```cmake
# In the tracker's CMakeLists.txt:
add_subdirectory(third_party/taktful)
target_link_libraries(tracker PRIVATE taktful)
```

The vendored `taktful` is built statically. The tracker can use either CMake or the Makefile approach above; both produce `libtaktful.a`.

---

## 16. Appendix B — Glossary

- **BPM** — Beats per minute.
- **Chroma** — A 12-element feature vector representing energy in each of the 12 pitch classes.
- **Constant-Q Transform (CQT)** — A spectral transform where bins are spaced logarithmically in frequency (rather than linearly, as in FFT). Used by libKeyFinder. We do not use CQT directly; we approximate it by mapping FFT bins to log-frequency.
- **DSP** — Digital signal processing.
- **HPCP** — Harmonic Pitch Class Profile. A chroma variant introduced by Gómez (2006) that uses spectral peaks rather than bin sums.
- **MIR** — Music Information Retrieval. The academic field this library belongs to.
- **PCP** — Pitch Class Profile. Synonymous with chroma in many contexts.
- **PCMF** — Pulse-code modulation format. The on-disk format of WAV files.
- **RIFF** — Resource Interchange File Format. The container format used by WAV.
- **STFT** — Short-Time Fourier Transform. The FFT applied to overlapping windowed frames of audio.
- **Vamp** — A plugin framework for audio feature extraction, used by Sonic Visualiser.
