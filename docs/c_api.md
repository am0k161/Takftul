# C API reference

Header: `include/taktful/c_api.h`. Language: C99-compatible `extern "C"`.
Link `libtaktful.a` plus the C++ runtime (`-lstdc++` or `-lc++`).

All functions return `taktful_error`. `TAKTFUL_OK` is 0. Strings returned by
`taktful_key_name`, `taktful_key_camelot`, `taktful_error_string`, `taktful_version`,
and the `key_name` / `camelot` fields of `taktful_result` are static — do not free.

---

## Errors

```c
typedef enum taktful_error {
    TAKTFUL_OK = 0,
    TAKTFUL_ERR_FILE_NOT_FOUND = 1,
    TAKTFUL_ERR_NOT_A_WAV = 2,
    TAKTFUL_ERR_UNSUPPORTED_FORMAT = 3,
    TAKTFUL_ERR_UNSUPPORTED_BIT_DEPTH = 4,
    TAKTFUL_ERR_OUT_OF_MEMORY = 5,
    TAKTFUL_ERR_TRUNCATED = 6,
    TAKTFUL_ERR_INVALID_ARG = 7,
    TAKTFUL_ERR_ANALYSIS = 8
} taktful_error;
```

`const char* taktful_error_string(taktful_error err);`

---

## Keys

```c
typedef enum taktful_key {
    TAKTFUL_KEY_C_MAJ = 0, ... TAKTFUL_KEY_B_MAJ,
    TAKTFUL_KEY_C_MIN, ...     TAKTFUL_KEY_B_MIN,
    TAKTFUL_KEY_UNKNOWN
} taktful_key;
```

```c
const char* taktful_key_name(taktful_key key);     /* "A minor" */
const char* taktful_key_camelot(taktful_key key);  /* "8A" */
const char* taktful_version(void);              /* "1.0.1" */
```

---

## Parameters

Call `taktful_params_default(&p)` then override fields. Zero / NULL `params` on
`taktful_analyze*` means "use defaults".

| Field | Default |
|---|---|
| `fft_size` | 4096 |
| `hop_size` | 512 |
| `target_sample_rate` | 11025 |
| `onset_method` | `TAKTFUL_ONSET_SPECTRAL_FLUX_HFC` |
| `tempo_min_bpm` / `tempo_max_bpm` | 60 / 200 |
| `enable_dp_refinement` | 1 |
| `chroma_ref_freq` | 440 |
| `chroma_method` | `TAKTFUL_CHROMA_HPCP` |
| `apply_pre_filter` | 1 |
| `hpcp_peak_threshold` | 1e-4 |
| `key_profiles` | `TAKTFUL_PROFILES_TEMPERLEY1999` |
| `return_alternatives` | 1 |
| `num_alternatives` | 5 |
| `fast_mode` | 0 |

`fast_mode` forces DP off and whitening off.

---

## WAV

```c
taktful_error taktful_load_wav(const char* path, taktful_wav* out);
taktful_error taktful_load_wav_mem(const void* data, size_t nbytes, taktful_wav* out);
void       taktful_wav_free(taktful_wav* wav);
```

`taktful_wav.samples` is a heap array of mono float `[-1, 1]`. Always `taktful_wav_free`.

---

## Analysis

```c
taktful_error taktful_analyze(const taktful_wav* wav, const taktful_params* params, taktful_result* out);
taktful_error taktful_analyze_buffer(const float* samples, uint32_t n_samples, uint32_t sample_rate,
                               const taktful_params* params, taktful_result* out);
void       taktful_result_free(taktful_result* result);
```

`taktful_result.beat_times_sec` is allocated only when DP is enabled and beats are found.
`alternatives` is an inline array of up to 24 `taktful_alt` (`key`, `confidence`, `camelot`).

---

## Streaming

```c
taktful_stream* taktful_stream_create(const taktful_params* params);
void         taktful_stream_reset(taktful_stream* s);
taktful_error   taktful_stream_push(taktful_stream* s, const float* samples, uint32_t n, uint32_t sample_rate);
taktful_error   taktful_stream_finish(taktful_stream* s, taktful_result* out);
void         taktful_stream_destroy(taktful_stream* s);
```

Sample rate must stay constant between `create`/`reset` and `finish`. `create` returns
NULL on allocation failure.
