# Golden tests

Each WAV in `corpus/` (generated at test time or supplied by the host) may be paired with a `.expected.json` file.

The CLI `analyze_wav --json` output is compared with a tolerance of:

- tempo: +/- 0.5 BPM (or +/- 4% for real-world tracks)
- key: exact match, or relative major/minor accepted for synthetic triads
- confidence: +/- 0.05

## Corpus target (~30 files)

- 5 synthetic click tracks at known BPMs (60, 90, 120, 140, 160)
- 5 simple melodies with known keys
- 10 electronic / tracker tracks from public-domain sources (not redistributed here)
- 10 tracks from MIREX / MTG Key datasets (paths only; datasets are not redistributed)

## Black-box reference

GPL tools (aubio, libKeyFinder, Essentia) may be run on the same WAV files to compare *outputs*. That is black-box use and does not contaminate this codebase. Do not copy source from those projects.

Unit tests in `tests/unit` generate synthetic click tracks and triads on the fly and cover the synthetic golden cases.

Generate a local synthetic corpus and score it:

```bash
mkdir -p /tmp/taktful-corpus
run_benchmark --synth --no-dp /tmp/taktful-corpus
```

For Extended Ballroom / MTG Key, point `run_benchmark` at a directory of WAV files each paired with a `.expected.json` containing `"bpm"` and/or `"key"`.
