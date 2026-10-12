# keeb

Lightweight MIT-licensed C++14 library for musical **key** and **BPM** detection from WAV files.

Designed for embedding in a music tracker on Linux handhelds (ARM Cortex-A class). Zero runtime dependencies, single static library, no machine learning.

## Features

- WAV decode: PCM 8/16/24/32-bit integer and 32-bit float, RF64, in-memory load, mono mixdown
- Tempo via spectral-flux onset + autocorrelation, optional Ellis DP beat tracking
- Key via HPCP / CLP chroma + Krumhansl–Schmuckler with Temperley profiles
- Ranked alternative keys, Camelot wheel labels, confidence scores
- High-level `analyze()`, sectioned analysis, streaming `StreamAnalyzer`
- C API (`keeb/c_api.h`) for C, Rust, Python, Lua FFI
- CLI tools: `keeb`, `analyze_wav`, `bpm_probe`, `key_probe`, `run_benchmark`

## Build

C++14 compiler required. CMake (primary) or a standalone Makefile.

### CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DKeeB_BUILD_CLI=ON -DKeeB_BUILD_TESTS=ON
cmake --build . -j
ctest --output-on-failure
```

Options:

- `-DKeeB_BUILD_CLI=ON` — build CLI tools including `run_benchmark` (default OFF)
- `-DKeeB_BUILD_TESTS=ON` — build unit + integration tests (default OFF)
- `-DKeeB_USE_KISSFFT=ON` — vendor kiss_fft instead of the in-house FFT (default OFF)
- `-DKeeB_BUILD_STATIC=ON` — produce `libkeeb.a` (default ON)

### Makefile

```bash
make -j lib
make -j cli
make -j tests
```

Override the compiler with `make CXX=clang++`.

## Usage

```cpp
#include <keeb/keeb.h>

keeb::WavData wav;
if (keeb::load_wav("track.wav", wav) != keeb::WavError::OK) {
    return 1;
}
keeb::AnalysisResult r = keeb::analyze(wav);
std::printf("%s  %.1f BPM\n", keeb::to_string(r.key.key), r.tempo.bpm);
```

From an existing float buffer:

```cpp
keeb::AnalysisResult r = keeb::analyze_buffer(samples, n_samples, sample_rate);
```

CLI (simple tester):

```bash
keeb track.wav
keeb
```

More tools:

```bash
analyze_wav --json track.wav
bpm_probe track.wav
key_probe track.wav
run_benchmark --synth /tmp/keeb-corpus
```

Embed in another CMake project:

```cmake
set(KeeB_BUILD_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/keeb)
target_link_libraries(my_app PRIVATE keeb)
```

## Documentation

- `docs/integration.md` — embed in C++, C, CMake, Make, Rust, Python, Lua, trackers
- `docs/api.md` — C++ public API
- `docs/c_api.md` — C ABI
- `docs/cli.md` — command-line tools
- `docs/tuning.md` — parameters, octave errors, fast mode
- `docs/dev_plan.md` — algorithms, licensing, roadmap

## Constraints (v1.0)

- WAV PCM / IEEE float / RF64 only (decode compressed formats yourself)
- 12-TET keys only
- Offline analysis (streaming is chunked-input, not real-time)
- No ML, no GPU, no external DSP libraries
- Single-threaded internally; no exceptions

## License

MIT. See `LICENSE`.

## Acknowledgements

Algorithms reimplemented from published papers (not from GPL library source):

- Cooley & Tukey (1965) — radix-2 FFT
- Bello et al. (2005) — onset detection / spectral flux
- Ellis (2007) — DP beat tracking
- Davies & Plumbley (2007) — autocorrelation tempo
- Gomez (2006) — HPCP chroma
- Muller & Ewert (2011) — chroma log pitch (CLP)
- Krumhansl (1990), Temperley (1999, 2007) — key-finding profiles

GPL/AGPL tools (libKeyFinder, aubio, Essentia) may be used as black-box output references only.
