# keeb public API

Include either the umbrella header `<keeb/keeb.h>` or individual module headers.

## Types

### `WavData`

Decoded mono audio in `[-1, 1]`.

- `uint32_t sample_rate`
- `uint16_t channels` — original channel count before mixdown
- `uint16_t bits_per_sample`
- `std::vector<float> samples` — mono mixdown, one sample per frame

### `WavError`

`OK`, `FileNotFound`, `NotAWav`, `UnsupportedFormat`, `UnsupportedBitDepth`, `OutOfMemory`, `Truncated`.

### `Params`

Tunables. Defaults match handheld / electronic-music use.

| Field | Default | Notes |
|---|---|---|
| `fft_size` | 4096 | Power of two, 256..16384 |
| `hop_size` | 512 | Must be `<= fft_size` |
| `target_sample_rate` | 11025 | Internal downsample |
| `onset_method` | `SpectralFluxHFC` | Or `SpectralFlux` |
| `tempo_min_bpm` / `tempo_max_bpm` | 60 / 200 | Search range |
| `enable_dp_refinement` | true | Ellis 2007 beat tracker |
| `chroma_ref_freq` | 440 | A4; use 432 for non-standard tuning |
| `use_hpcp` | true | false = naive PCP |
| `chroma_method` | `Hpcp` | Also `NaivePcp`, `Clp` |
| `apply_pre_filter` | true | Spectral whitening |
| `hpcp_peak_threshold` | 1e-4 | Peak-picking floor |
| `key_profiles` | `Temperley1999` | Also `KrumhanslOriginal`, `Temperley2007` |
| `return_alternatives` | true | Fill `KeyResult::alternatives` |
| `num_alternatives` | 5 | 1..24 |
| `fast_mode` | false | Disables DP and whitening |

### `Key`

24 keys (`CMaj`..`BMaj`, `CMin`..`BMin`) plus `Unknown`.

`to_string(Key)` returns `"A minor"`, `"C major"`, etc.
`to_camelot(Key)` returns Camelot labels (`"8A"`, `"8B"`, ...).

### Results

- `TempoResult`: `bpm`, `confidence` (0..1), optional `beat_times_sec`
- `KeyResult`: `key`, `confidence`, `camelot`, `alternatives`
- `AnalysisResult`: `tempo` + `key`

## Functions

```cpp
WavError load_wav(const char* path, WavData& out);

AnalysisResult analyze(const WavData& audio, const Params& params = Params());

IntermediateFeatures compute_features(const WavData& audio, const Params& params = Params());

bool validate_params(Params& params);

WavError load_wav_mem(const void* data, size_t nbytes, WavData& out);

AnalysisResult analyze_buffer(const float* samples, uint32_t n, uint32_t sr, const Params& params = Params());

std::vector<AnalysisResult> analyze_sections(const WavData& audio, float section_sec, const Params& params = Params());
```

`analyze()` never throws. Silent or empty input returns `bpm = 0` and `Key::Unknown`.

`compute_features()` returns per-hop onset envelope and 12-bin chroma for visualisation.

`StreamAnalyzer` accepts chunked `push()` then `finish()`. See `<keeb/stream.h>`.

On ARM, FFT / magnitude / window / RMS use NEON when `__ARM_NEON` is set (`-DKeeB_ENABLE_NEON=OFF` to disable).

## C API

`<keeb/c_api.h>` is `extern "C"` for Rust / Python / Lua:

```c
keeb_wav wav;
keeb_load_wav("track.wav", &wav);
keeb_params p;
keeb_params_default(&p);
keeb_result r;
keeb_analyze(&wav, &p, &r);
keeb_result_free(&r);
keeb_wav_free(&wav);
```

Streaming: `keeb_stream_create` / `keeb_stream_push` / `keeb_stream_finish` / `keeb_stream_destroy`.

## Embedding

```cmake
add_subdirectory(third_party/keeb)
target_link_libraries(tracker PRIVATE keeb)
```

The library is a static archive by default (`libkeeb.a`).
