#ifndef KEEB_C_API_H
#define KEEB_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum keeb_key {
    KEEB_KEY_C_MAJ = 0,  KEEB_KEY_CS_MAJ, KEEB_KEY_D_MAJ,  KEEB_KEY_DS_MAJ,
    KEEB_KEY_E_MAJ,      KEEB_KEY_F_MAJ,  KEEB_KEY_FS_MAJ, KEEB_KEY_G_MAJ,
    KEEB_KEY_GS_MAJ,     KEEB_KEY_A_MAJ,  KEEB_KEY_AS_MAJ, KEEB_KEY_B_MAJ,
    KEEB_KEY_C_MIN,      KEEB_KEY_CS_MIN, KEEB_KEY_D_MIN,  KEEB_KEY_DS_MIN,
    KEEB_KEY_E_MIN,      KEEB_KEY_F_MIN,  KEEB_KEY_FS_MIN, KEEB_KEY_G_MIN,
    KEEB_KEY_GS_MIN,     KEEB_KEY_A_MIN,  KEEB_KEY_AS_MIN, KEEB_KEY_B_MIN,
    KEEB_KEY_UNKNOWN
} keeb_key;

typedef enum keeb_onset_method {
    KEEB_ONSET_SPECTRAL_FLUX = 0,
    KEEB_ONSET_SPECTRAL_FLUX_HFC = 1
} keeb_onset_method;

typedef enum keeb_key_profiles {
    KEEB_PROFILES_KRUMHANSL = 0,
    KEEB_PROFILES_TEMPERLEY1999 = 1,
    KEEB_PROFILES_TEMPERLEY2007 = 2
} keeb_key_profiles;

typedef enum keeb_chroma_method {
    KEEB_CHROMA_HPCP = 0,
    KEEB_CHROMA_NAIVE_PCP = 1,
    KEEB_CHROMA_CLP = 2
} keeb_chroma_method;

typedef struct keeb_wav {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    float* samples;
    uint32_t n_samples;
} keeb_wav;

typedef struct keeb_params {
    uint32_t fft_size;
    uint32_t hop_size;
    uint32_t target_sample_rate;
    keeb_onset_method onset_method;
    float tempo_min_bpm;
    float tempo_max_bpm;
    int enable_dp_refinement;
    float chroma_ref_freq;
    keeb_chroma_method chroma_method;
    int apply_pre_filter;
    float hpcp_peak_threshold;
    keeb_key_profiles key_profiles;
    int return_alternatives;
    uint8_t num_alternatives;
    int fast_mode;
} keeb_params;

typedef struct keeb_alt {
    keeb_key key;
    float confidence;
    const char* camelot;
} keeb_alt;

typedef struct keeb_result {
    float bpm;
    float tempo_confidence;
    float* beat_times_sec;
    uint32_t n_beats;
    keeb_key key;
    float key_confidence;
    const char* key_name;
    const char* camelot;
    keeb_alt alternatives[24];
    uint32_t n_alternatives;
} keeb_result;

typedef struct keeb_stream keeb_stream;

const char* keeb_version(void);
const char* keeb_error_string(keeb_error err);
const char* keeb_key_name(keeb_key key);
const char* keeb_key_camelot(keeb_key key);

void keeb_params_default(keeb_params* params);

keeb_error keeb_load_wav(const char* path, keeb_wav* out);
keeb_error keeb_load_wav_mem(const void* data, size_t nbytes, keeb_wav* out);
void keeb_wav_free(keeb_wav* wav);

keeb_error keeb_analyze(const keeb_wav* wav, const keeb_params* params, keeb_result* out);
keeb_error keeb_analyze_buffer(const float* samples, uint32_t n_samples, uint32_t sample_rate,
                               const keeb_params* params, keeb_result* out);
void keeb_result_free(keeb_result* result);

keeb_stream* keeb_stream_create(const keeb_params* params);
void keeb_stream_reset(keeb_stream* s);
keeb_error keeb_stream_push(keeb_stream* s, const float* samples, uint32_t n, uint32_t sample_rate);
keeb_error keeb_stream_finish(keeb_stream* s, keeb_result* out);
void keeb_stream_destroy(keeb_stream* s);

#ifdef __cplusplus
}
#endif

#endif
