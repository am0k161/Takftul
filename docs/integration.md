# Integrating taktful into an application

taktful is a single static library (`libtaktful.a`) with no runtime dependencies beyond the C++
standard library. C++14 is required. The public surface is:

- C++: `<taktful/taktful.h>` (umbrella) or the module headers
- C: `<taktful/c_api.h>` (`extern "C"`), for C, Rust, Python, Lua, and anything that can FFI C

The library never throws. Errors are return codes. `analyze()` on silence returns
`bpm = 0` and `Key::Unknown`.

---

## 1. Vendor the sources

Clone or subtree/submodule the repo into your project, for example `third_party/taktful`.

```bash
git submodule add https://github.com/am0k161/Takftul.git third_party/taktful
```

Or copy `include/`, `src/`, `data/`, `CMakeLists.txt` (and optionally `Makefile`) into
your tree. You do not need `cli/`, `tests/`, or `docs/` at runtime.

---

## 2. CMake (recommended)

### As a subdirectory

```cmake
# in your project's CMakeLists.txt
set(Taktful_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(Taktful_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(Taktful_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/taktful)

target_link_libraries(my_app PRIVATE taktful)
```

Then:

```cpp
#include <taktful/taktful.h>
```

CMake already exports include path `include/` so `<taktful/taktful.h>` resolves.

### Prebuilt archive

```cmake
add_library(taktful STATIC IMPORTED)
set_target_properties(taktful PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/taktful/libtaktful.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/third_party/taktful/include
)
target_link_libraries(my_app PRIVATE taktful)
```

If you compiled taktful as C++ and your app is C, also link the C++ runtime:

```cmake
target_link_libraries(my_c_app PRIVATE taktful stdc++)
```

---

## 3. Makefile / raw compiler

```make
TAKTFUL = third_party/taktful
CXXFLAGS += -std=c++14 -I$(TAKTFUL)/include
LDFLAGS  += $(TAKTFUL)/libtaktful.a

# or build taktful in-tree:
$(TAKTFUL)/libtaktful.a:
	$(MAKE) -C $(TAKTFUL) lib
```

From the taktful tree itself:

```bash
make -j lib          # produces libtaktful.a
```

Link example:

```bash
g++ -std=c++14 -O2 -Ithird_party/taktful/include app.cpp third_party/taktful/libtaktful.a -o app
```

For a C program:

```bash
cc -c app.c -Ithird_party/taktful/include
g++ app.o third_party/taktful/libtaktful.a -o app
```

(`g++` as the linker pulls in libstdc++.)

---

## 4. C++ application

High-level, whole-file:

```cpp
#include <taktful/taktful.h>
#include <cstdio>

int main() {
    taktful::WavData wav;
    taktful::WavError e = taktful::load_wav("loop.wav", wav);
    if (e != taktful::WavError::OK) {
        std::fprintf(stderr, "load failed\n");
        return 1;
    }

    taktful::Params p;                 // defaults are fine for electronic / tracker music
    p.fast_mode = false;
    taktful::AnalysisResult r = taktful::analyze(wav, p);

    std::printf("BPM  %.1f  (%0.2f)\n", r.tempo.bpm, r.tempo.confidence);
    std::printf("Key  %s  [%s]  (%0.2f)\n",
                taktful::to_string(r.key.key),
                taktful::to_camelot(r.key.key),
                r.key.confidence);
    return 0;
}
```

Already have a float buffer (your decoder, tracker engine, etc.):

```cpp
taktful::AnalysisResult r = taktful::analyze_buffer(samples, n_samples, sample_rate);
```

`samples` is mono `float` in `[-1, 1]`. Stereo must be mixed down first (average the
channels) — `load_wav` does that for WAV files.

Per-section analysis (e.g. every 30 seconds):

```cpp
std::vector<taktful::AnalysisResult> parts = taktful::analyze_sections(wav, 30.0f);
```

UI visualisation (onset envelope + 12-bin chroma per hop):

```cpp
taktful::IntermediateFeatures f = taktful::compute_features(wav);
// f.onset_envelope[t]
// f.chroma[t][0..11]   // C, C#, ..., B
```

Streaming / chunked input (live capture, tracker loop while the file is still loading):

```cpp
taktful::StreamAnalyzer s;
s.push(chunk, n, sample_rate);     // call repeatedly
taktful::AnalysisResult r = s.finish();
s.reset();                         // reuse
```

`push()` requires a constant sample rate for the lifetime of the analyzer.

---

## 5. C application

```c
#include <taktful/c_api.h>
#include <stdio.h>

int main(void) {
    taktful_wav wav;
    taktful_error e = taktful_load_wav("loop.wav", &wav);
    if (e != TAKTFUL_OK) {
        fprintf(stderr, "%s\n", taktful_error_string(e));
        return 1;
    }

    taktful_params p;
    taktful_params_default(&p);

    taktful_result r;
    e = taktful_analyze(&wav, &p, &r);
    if (e == TAKTFUL_OK) {
        printf("%.1f BPM   %s [%s]\n", r.bpm, r.key_name, r.camelot);
    }

    taktful_result_free(&r);
    taktful_wav_free(&wav);
    return 0;
}
```

From an existing float buffer:

```c
taktful_analyze_buffer(samples, n_samples, sample_rate, &p, &r);
```

Streaming:

```c
taktful_stream* s = taktful_stream_create(&p);
taktful_stream_push(s, chunk, n, sample_rate);
taktful_stream_finish(s, &r);
taktful_stream_destroy(s);
```

`taktful_result.beat_times_sec` is heap-allocated when DP refinement is on; always call
`taktful_result_free`. `key_name` and `camelot` point at static strings — do not free them.

---

## 6. Music tracker (typical embed)

Suggested layout inside a tracker:

1. On file load (or "detect key/BPM" action), call `load_wav` or `analyze_buffer` on the
   mixdown of the current song / sample.
2. Show `to_string(key)` and BPM in the status bar. Offer Camelot (`to_camelot`) for DJs.
3. Draw `compute_features().onset_envelope` as a waveform overlay and chroma as a 12-bin
   piano-roll heat map.
4. For live loop preview, construct a `StreamAnalyzer`, `push` blocks from the audio
   callback's ring buffer on a worker thread, `finish()` when the user stops. Do not call
   taktful from the real-time audio thread — analysis is offline and allocates.

Fast re-run while the user scrubs:

```cpp
p.fast_mode = true;          // skips Ellis DP and spectral whitening
```

Handheld note: peak working set excluding the audio buffer is on the order of 1–2 MB for
a 4-minute track at the default hop. Downsampling to 11025 Hz is already the default.

---

## 7. Rust (FFI)

`build.rs` (cc crate) or link a prebuilt `libtaktful.a`. Because taktful is C++, compile it
with a C++ compiler and link `libstdc++` / `libc++`.

```toml
# Cargo.toml
[build-dependencies]
cc = "1"
```

```rust
// build.rs
fn main() {
    cc::Build::new()
        .cpp(true)
        .flag("-std=c++14")
        .include("third_party/taktful/include")
        .files(&[
            "third_party/taktful/src/wav_io.cpp",
            "third_party/taktful/src/dsp.cpp",
            "third_party/taktful/src/fft.cpp",
            "third_party/taktful/src/onset.cpp",
            "third_party/taktful/src/tempo.cpp",
            "third_party/taktful/src/chroma.cpp",
            "third_party/taktful/src/key.cpp",
            "third_party/taktful/src/api.cpp",
            "third_party/taktful/src/stream.cpp",
            "third_party/taktful/src/c_api.cpp",
        ])
        .compile("taktful");
    println!("cargo:rustc-link-lib=stdc++");
}
```

```rust
// src/lib.rs  (minimal)
#[repr(C)]
struct KeebResult {
    bpm: f32,
    tempo_confidence: f32,
    beat_times_sec: *mut f32,
    n_beats: u32,
    key: i32,
    key_confidence: f32,
    key_name: *const i8,
    camelot: *const i8,
    // alternatives omitted for brevity; see include/taktful/c_api.h
}

extern "C" {
    fn taktful_analyze_buffer(
        samples: *const f32,
        n_samples: u32,
        sample_rate: u32,
        params: *const std::ffi::c_void,
        out: *mut KeebResult,
    ) -> i32;
    fn taktful_result_free(result: *mut KeebResult);
}
```

Prefer `bindgen` on `include/taktful/c_api.h` rather than hand-writing the full layout.

---

## 8. Python (ctypes)

Build `libtaktful.a` (or a shared lib with `-DTaktful_BUILD_STATIC=OFF`) and wrap the C API:

```python
import ctypes

lib = ctypes.CDLL("libtaktful.so")  # or load via gcc -shared

class KeebResult(ctypes.Structure):
    _fields_ = [
        ("bpm", ctypes.c_float),
        ("tempo_confidence", ctypes.c_float),
        ("beat_times_sec", ctypes.POINTER(ctypes.c_float)),
        ("n_beats", ctypes.c_uint32),
        ("key", ctypes.c_int),
        ("key_confidence", ctypes.c_float),
        ("key_name", ctypes.c_char_p),
        ("camelot", ctypes.c_char_p),
        # ...
    ]

lib.taktful_load_wav.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
```

A thin `cffi` / `pybind11` wrapper is cleaner for production; ctypes is enough for tests.

---

## 9. Lua

Use LuaJIT FFI against `c_api.h`, or bind with Lua C API the same way as any C library.
Link `libtaktful.a` plus libstdc++ into the host (the tracker, or a Lua C module `.so`).

```lua
local ffi = require("ffi")
ffi.cdef[[
  typedef struct taktful_result taktful_result;
  const char* taktful_version(void);
]]
local taktful = ffi.load("taktful")
print(ffi.string(taktful.taktful_version()))
```

Copy the relevant prototypes from `include/taktful/c_api.h` into `ffi.cdef`.

---

## 10. Threading and memory

- The library is single-threaded internally. Multiple `analyze()` / `StreamAnalyzer`
  instances may run in parallel on different threads (no process-wide mutable state
  except a `thread_local` FFT twiddle cache).
- Do not call taktful from a real-time audio callback. Heap allocations happen inside
  `analyze()` / `push()`.
- No exceptions, no RTTI requirement, no `dynamic_cast`.
- Working set (excluding the input buffer) is bounded: FFT buffer, onset envelope,
  chroma matrix. See the development plan in `docs/dev_plan.md` for the 4-minute budget.
- ARM builds use NEON for FFT butterflies, magnitude, windowing, and RMS when
  `__ARM_NEON` is set. Disable with `-DTaktful_ENABLE_NEON=OFF` or `make NEON=0`.

---

## 11. What taktful will not do

- Decode MP3 / Ogg / FLAC (decode first, pass PCM or WAV).
- Real-time guaranteed latency.
- Chord transcription, source separation, non-12-TET keys.
- Neural / ML models.

Those are host responsibilities by design, so the tracker stays copyleft-clean and small.
