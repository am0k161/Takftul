# CLI tools

All tools are built with `-DKeeB_BUILD_CLI=ON` (CMake) or `make cli` (Makefile).
Binaries land in `build/` (CMake) or the repo root (Makefile).

Exit codes (shared):

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | file not found |
| 2 | not a valid WAV |
| 3 | unsupported WAV format |
| 4 | out of memory |
| 5 | analysis failed (silent / too short) |
| 64 | bad command-line arguments |

Input: PCM WAV (8/16/24/32-bit integer or 32-bit float), including RF64. Stereo and surround are mixed down to mono.

---

## `keeb` — simple tester

Interactive one-shot analysis. Path as an argument, or prompt if omitted.

```bash
keeb track.wav
keeb
```

Plain-text output:

```text
file:     track.wav
duration: 180.00 s
tempo:    128.0 BPM  (confidence 0.82)
key:      A minor  [8A]  (confidence 0.74)
alts:     A minor 0.74, C major 0.69, E minor 0.55
```

---

## `analyze_wav` — full pipeline

```bash
analyze_wav [options] <path-to-wav>
```

| Option | Effect |
|---|---|
| `--json` | JSON output (default) |
| `--text` | Human-readable text |
| `--fft-size N` | FFT size, power of two 256..16384 (default 4096) |
| `--hop-size N` | Hop size (default 512) |
| `--no-dp` | Disable Ellis DP beat refinement |
| `--fast` | Fast mode (no DP, no whitening) |
| `--profiles NAME` | `krumhansl` / `temperley1999` / `temperley2007` |
| `--alternatives N` | Number of alternative keys (default 5) |
| `--stats` | Peak heap + CPU time |
| `--verbose` | Intermediate stages on stderr |
| `--help` | Usage |

JSON fields: `file`, `duration_sec`, `sample_rate`, `tempo.{bpm,confidence,beat_times_sec}`, `key.{key,camelot,confidence,alternatives}`, optional `stats`.

---

## `bpm_probe` — tempo only

```bash
bpm_probe [options] <path-to-wav>
```

| Option | Effect |
|---|---|
| `--min BPM` | Minimum tempo (default 60) |
| `--max BPM` | Maximum tempo (default 200) |
| `--no-dp` | Disable DP |
| `--text` | Text output (default) |
| `--json` | JSON output |
| `--onset-csv FILE` | Dump onset envelope |
| `--acorr-csv FILE` | Dump autocorrelation curve |

CSV files are RFC-4180 with a header row and 6 decimal places (plottable in gnuplot / matplotlib).

---

## `key_probe` — key only

```bash
key_probe [options] <path-to-wav>
```

| Option | Effect |
|---|---|
| `--profiles NAME` | `krumhansl` / `temperley1999` / `temperley2007` |
| `--ref-freq HZ` | A4 reference (default 440) |
| `--naive-pcp` | Naive pitch-class profile instead of HPCP |
| `--clp` | Chroma log pitch instead of HPCP |
| `--chroma-csv FILE` | Song-level 12-dim PCP |
| `--per-frame-csv FILE` | Per-frame chroma matrix |

---

## `run_benchmark` — corpus scoring

```bash
run_benchmark [--synth] [--no-dp] [--clp] [--json] <dir>
```

`--synth` writes click tracks (60/90/120/140/160 BPM) and major triads into `<dir>`, then scores them.

For a real corpus, put WAV files next to `.expected.json` files:

```json
{"bpm": 128.0, "key": "A minor"}
```

Either field may be omitted. Tempo is accepted within 0.5 BPM or 4%, including half/double octave matches. Key is scored strict and relative-major/minor.
