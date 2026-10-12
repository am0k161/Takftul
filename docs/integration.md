# Integrating keeb into an application

keeb is a single static library (`libkeeb.a`) with no runtime dependencies beyond the C++
standard library. C++14 is required. The public surface is:

- C++: `<keeb/keeb.h>` (umbrella) or the module headers
- C: `<keeb/c_api.h>` (`extern "C"`), for C, Rust, Python, Lua, and anything that can FFI C

The library never throws. Errors are return codes. `analyze()` on silence returns
`bpm = 0` and `Key::Unknown`.

---

## 1. Vendor the sources

Clone or subtree/submodule the repo into your project, for example `third_party/keeb`.

```bash
git submodule add https://github.com/am0k161/Takftul.git third_party/keeb
```

Or copy `include/`, `src/`, `data/`, `CMakeLists.txt` (and optionally `Makefile`) into
your tree. You do not need `cli/`, `tests/`, or `docs/` at runtime.

---

## 2. CMake (recommended)

### As a subdirectory

```cmake
# in your project's CMakeLists.txt
set(KeeB_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(KeeB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(KeeB_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/keeb)

target_link_libraries(my_app PRIVATE keeb)
```

Then:

```cpp
#include <keeb/keeb.h>
```

CMake already exports include path `include/` so `<keeb/keeb.h>` resolves.

### Prebuilt archive

```cmake
add_library(keeb STATIC IMPORTED)
set_target_properties(keeb PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/keeb/libkeeb.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/third_party/keeb/include
)
target_link_libraries(my_app PRIVATE keeb)
```

If you compiled keeb as C++ and your app is C, also link the C++ runtime:

```cmake
target_link_libraries(my_c_app PRIVATE keeb stdc++)
```

---

## 3. Makefile / raw compiler

```make
KEEB = third_party/keeb
CXXFLAGS += -std=c++14 -I$(KEEB)/include
LDFLAGS  += $(KEEB)/libkeeb.a

# or build keeb in-tree:
$(KEEB)/libkeeb.a:
	$(MAKE) -C $(KEEB) lib
```

From the keeb tree itself:

```bash
make -j lib          # produces libkeeb.a
```

Link example:

```bash
g++ -std=c++14 -O2 -Ithird_party/keeb/include app.cpp third_party/keeb/libkeeb.a -o app
```

For a C program:

```bash
cc -c app.c -Ithird_party/keeb/include
g++ app.o third_party/keeb/libkeeb.a -o app
```

(`g++` as the linker pulls in libstdc++.)

---

## 4. C++ application

High-level, whole-file:

```cpp
#include <keeb/keeb.h>
#include <cstdio>

int main() {
    keeb::WavData wav;
    keeb::WavError e = keeb::load_wav("loop.wav", wav);
    if (e != keeb::WavError::OK) {
        std::fprintf(stderr, "load failed\n");
        return 1;
    }

    keeb::Params p;                 // defaults are fine for electronic / tracker music
    p.fast_mode = false;
    keeb::AnalysisResult r = keeb::analyze(wav, p);

    std::printf("BPM  %.1f  (%0.2f)\n", r.tempo.bpm, r.tempo.confidence);
    std::printf("Key  %s  [%s]  (%0.2f)\n",
                keeb::to_string(r.key.key),
                keeb::to_camelot(r.key.key),
                r.key.confidence);
    return 0;
}
```

Already have a float buffer (your decoder, tracker engine, etc.):

```cpp
keeb::AnalysisResult r = keeb::analyze_buffer(samples, n_samples, sample_rate);
```

`samples` is mono `float` in `[-1, 1]`. Stereo must be mixed down first (average the
channels) — `load_wav` does that for WAV files.

Per-section analysis (e.g. every 30 seconds):

```cpp
std::vector<keeb::AnalysisResult> parts = keeb::analyze_sections(wav, 30.0f);
```

UI visualisation (onset envelope + 12-bin chroma per hop):

```cpp
keeb::IntermediateFeatures f = keeb::compute_features(wav);
// f.onset_envelope[t]
// f.chroma[t][0..11]   // C, C#, ..., B
```

Streaming / chunked input (live capture, tracker loop while the file is still loading):

```cpp
keeb::StreamAnalyzer s;
s.push(chunk, n, sample_rate);     // call repeatedly
keeb::AnalysisResult r = s.finish();
s.reset();                         // reuse
```

`push()` requires a constant sample rate for the lifetime of the analyzer.

---

## 5. C application

```c
#include <keeb/c_api.h>
#include <stdio.h>

int main(void) {
    keeb_wav wav;
    keeb_error e = keeb_load_wav("loop.wav", &wav);
    if (e != KEEB_OK) {
        fprintf(stderr, "%s\n", keeb_error_string(e));
        return 1;
    }

    keeb_params p;
    keeb_params_default(&p);

    keeb_result r;
    e = keeb_analyze(&wav, &p, &r);
    if (e == KEEB_OK) {
        printf("%.1f BPM   %s [%s]\n", r.bpm, r.key_name, r.camelot);
    }

    keeb_result_free(&r);
    keeb_wav_free(&wav);
    return 0;
}
```

From an existing float buffer:

```c
keeb_analyze_buffer(samples, n_samples, sample_rate, &p, &r);
```

Streaming:

```c
keeb_stream* s = keeb_stream_create(&p);
keeb_stream_push(s, chunk, n, sample_rate);
keeb_stream_finish(s, &r);
keeb_stream_destroy(s);
```

`keeb_result.beat_times_sec` is heap-allocated when DP refinement is on; always call
`keeb_result_free`. `key_name` and `camelot` point at static strings — do not free them.

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
   keeb from the real-time audio thread — analysis is offline and allocates.

Fast re-run while the user scrubs:

```cpp
p.fast_mode = true;          // skips Ellis DP and spectral whitening
```

Handheld note: peak working set excluding the audio buffer is on the order of 1–2 MB for
a 4-minute track at the default hop. Downsampling to 11025 Hz is already the default.

---

## 7. Rust (FFI)

`build.rs` (cc crate) or link a prebuilt `libkeeb.a`. Because keeb is C++, compile it
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
        .include("third_party/keeb/include")
        .files(&[
            "third_party/keeb/src/wav_io.cpp",
            "third_party/keeb/src/dsp.cpp",
            "third_party/keeb/src/fft.cpp",
            "third_party/keeb/src/onset.cpp",
            "third_party/keeb/src/tempo.cpp",
            "third_party/keeb/src/chroma.cpp",
            "third_party/keeb/src/key.cpp",
            "third_party/keeb/src/api.cpp",
            "third_party/keeb/src/stream.cpp",
            "third_party/keeb/src/c_api.cpp",
        ])
        .compile("keeb");
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
    // alternatives omitted for brevity; see include/keeb/c_api.h
}

extern "C" {
    fn keeb_analyze_buffer(
        samples: *const f32,
        n_samples: u32,
        sample_rate: u32,
        params: *const std::ffi::c_void,
        out: *mut KeebResult,
    ) -> i32;
    fn keeb_result_free(result: *mut KeebResult);
}
```

Prefer `bindgen` on `include/keeb/c_api.h` rather than hand-writing the full layout.

---

## 8. Python (ctypes)

Build `libkeeb.a` (or a shared lib with `-DKeeB_BUILD_STATIC=OFF`) and wrap the C API:

```python
import ctypes

lib = ctypes.CDLL("libkeeb.so")  # or load via gcc -shared

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

lib.keeb_load_wav.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
```

A thin `cffi` / `pybind11` wrapper is cleaner for production; ctypes is enough for tests.

---

## 9. Lua

Use LuaJIT FFI against `c_api.h`, or bind with Lua C API the same way as any C library.
Link `libkeeb.a` plus libstdc++ into the host (the tracker, or a Lua C module `.so`).

```lua
local ffi = require("ffi")
ffi.cdef[[
  typedef struct keeb_result keeb_result;
  const char* keeb_version(void);
]]
local keeb = ffi.load("keeb")
print(ffi.string(keeb.keeb_version()))
```

Copy the relevant prototypes from `include/keeb/c_api.h` into `ffi.cdef`.

---

## 10. Threading and memory

- The library is single-threaded internally. Multiple `analyze()` / `StreamAnalyzer`
  instances may run in parallel on different threads (no process-wide mutable state
  except a `thread_local` FFT twiddle cache).
- Do not call keeb from a real-time audio callback. Heap allocations happen inside
  `analyze()` / `push()`.
- No exceptions, no RTTI requirement, no `dynamic_cast`.
- Working set (excluding the input buffer) is bounded: FFT buffer, onset envelope,
  chroma matrix. See the development plan in `docs/dev_plan.md` for the 4-minute budget.
- ARM builds use NEON for FFT butterflies, magnitude, windowing, and RMS when
  `__ARM_NEON` is set. Disable with `-DKeeB_ENABLE_NEON=OFF` or `make NEON=0`.

---

## 11. What keeb will not do

- Decode MP3 / Ogg / FLAC (decode first, pass PCM or WAV).
- Real-time guaranteed latency.
- Chord transcription, source separation, non-12-TET keys.
- Neural / ML models.

Those are host responsibilities by design, so the tracker stays copyleft-clean and small.
