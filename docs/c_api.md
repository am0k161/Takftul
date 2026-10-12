# C API reference

Header: `include/keeb/c_api.h`. Language: C99-compatible `extern "C"`.
Link `libkeeb.a` plus the C++ runtime (`-lstdc++` or `-lc++`).

All functions return `keeb_error`. `KEEB_OK` is 0. Strings returned by
`keeb_key_name`, `keeb_key_camelot`, `keeb_error_string`, `keeb_version`,
and the `key_name` / `camelot` fields of `keeb_result` are static — do not free.

---

## Errors

```c
typedef enum keeb_error {
    KEEB_OK = 0,
    KEEB_ERR_FILE_NOT_FOUND = 1,
    KEEB_ERR_NOT_A_WAV = 2,
    KEEB_ERR_UNSUPPORTED_FORMAT = 3,
    KEEB_ERR_UNSUPPORTED_BIT_DEPTH = 4,
    KEEB_ERR_OUT_OF_MEMORY = 5,
    KEEB_ERR_TRUNCATED = 6,
    KEEB_ERR_INVALID_ARG = 7,
    KEEB_ERR_ANALYSIS = 8
} keeb_error;
```

`const char* keeb_error_string(keeb_error err);`

---

## Keys

```c
typedef enum keeb_key {
    KEEB_KEY_C_MAJ = 0, ... KEEB_KEY_B_MAJ,
    KEEB_KEY_C_MIN, ...     KEEB_KEY_B_MIN,
    KEEB_KEY_UNKNOWN
} keeb_key;
```

```c
const char* keeb_key_name(keeb_key key);     /* "A minor" */
const char* keeb_key_camelot(keeb_key key);  /* "8A" */
const char* keeb_version(void);              /* "1.0.1" */
```

---

## Parameters

Call `keeb_params_default(&p)` then override fields. Zero / NULL `params` on
`keeb_analyze*` means "use defaults".

| Field | Default |
|---|---|
| `fft_size` | 4096 |
| `hop_size` | 512 |
| `target_sample_rate` | 11025 |
| `onset_method` | `KEEB_ONSET_SPECTRAL_FLUX_HFC` |
| `tempo_min_bpm` / `tempo_max_bpm` | 60 / 200 |
| `enable_dp_refinement` | 1 |
| `chroma_ref_freq` | 440 |
| `chroma_method` | `KEEB_CHROMA_HPCP` |
| `apply_pre_filter` | 1 |
| `hpcp_peak_threshold` | 1e-4 |
| `key_profiles` | `KEEB_PROFILES_TEMPERLEY1999` |
| `return_alternatives` | 1 |
| `num_alternatives` | 5 |
| `fast_mode` | 0 |

`fast_mode` forces DP off and whitening off.

---

## WAV

```c
keeb_error keeb_load_wav(const char* path, keeb_wav* out);
keeb_error keeb_load_wav_mem(const void* data, size_t nbytes, keeb_wav* out);
void       keeb_wav_free(keeb_wav* wav);
```

`keeb_wav.samples` is a heap array of mono float `[-1, 1]`. Always `keeb_wav_free`.

---

## Analysis

```c
keeb_error keeb_analyze(const keeb_wav* wav, const keeb_params* params, keeb_result* out);
keeb_error keeb_analyze_buffer(const float* samples, uint32_t n_samples, uint32_t sample_rate,
                               const keeb_params* params, keeb_result* out);
void       keeb_result_free(keeb_result* result);
```

`keeb_result.beat_times_sec` is allocated only when DP is enabled and beats are found.
`alternatives` is an inline array of up to 24 `keeb_alt` (`key`, `confidence`, `camelot`).

---

## Streaming

```c
keeb_stream* keeb_stream_create(const keeb_params* params);
void         keeb_stream_reset(keeb_stream* s);
keeb_error   keeb_stream_push(keeb_stream* s, const float* samples, uint32_t n, uint32_t sample_rate);
keeb_error   keeb_stream_finish(keeb_stream* s, keeb_result* out);
void         keeb_stream_destroy(keeb_stream* s);
```

Sample rate must stay constant between `create`/`reset` and `finish`. `create` returns
NULL on allocation failure.
