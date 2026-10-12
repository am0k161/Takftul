# Tuning taktful

Default parameters target electronic / tracker music on handheld CPUs.

## Tempo

- Octave errors (half / double BPM) are common. The library prefers the slower peak when two autocorrelation peaks are within 80% of each other and the faster estimate is above 130 BPM.
- Disable Ellis DP with `Params::enable_dp_refinement = false` or `--no-dp` for a cheaper pass.
- If ambient / drone input has no beat, expect `bpm = 0`, `confidence = 0`.
- Narrow `tempo_min_bpm` / `tempo_max_bpm` when the genre is known (e.g. 120–150 for house).

## Key

- `Temperley1999` is the default profile set. `KrumhanslOriginal` is more classical-oriented. `Temperley2007` is an alternative if 1999 underperforms on your corpus.
- Relative major/minor confusion (C major vs A minor) is expected for triads and diatonic loops. Check `alternatives`.
- `chroma_ref_freq` defaults to 440 Hz. Set 432 (or another A4) for non-standard tunings.
- If HPCP misses quiet pitched material, lower `hpcp_peak_threshold` or set `use_hpcp = false` (naive PCP).
- `apply_pre_filter` (spectral whitening) reduces timbre bias; turn off in `fast_mode`.
- `chroma_method = Clp` (chroma log pitch) is a fallback when HPCP is noisy. Enable with `key_probe --clp`.

## Speed vs accuracy

`Params::fast_mode = true` disables DP refinement and whitening. Use it when the tracker re-runs analysis on every loop.

Internal analysis sample rate is 11025 Hz. That covers piano range for key and cuts CPU about 4x vs 44.1 kHz. Hi-hat energy above ~5 kHz is reduced; if onset detection looks weak on cymbal-heavy loops, raise `target_sample_rate` toward 22050.

## FFT

`fft_size` 4096 / `hop_size` 512 is the default. A larger hop (2048) is faster but too coarse for tempo at 11025 Hz. Smaller FFT (2048) is faster; larger (8192) helps low-note key detection.

If the in-house radix-2 RFFT is wrong or slow, rebuild with `-DTaktful_USE_KISSFFT=ON` after vendoring KissFFT into `third_party/kiss_fft/`.

## ARM NEON

On ARM (`__ARM_NEON`), the FFT butterflies (stages with length >= 8), magnitude spectrum, window multiply, and RMS use NEON 4-wide float kernels. Scalar code remains the fallback for tails and for non-ARM hosts.

- CMake: `-DTaktful_ENABLE_NEON=ON` (default) or `OFF` to force scalar.
- Makefile: `make NEON=0` forces scalar. 32-bit ARM: `make NEON=1 ARM_FPU=neon`.
- aarch64 already has NEON; no extra FPU flag is required.

Scratch FFT buffers are cached per thread and size so STFT frames do not reallocate every hop.
