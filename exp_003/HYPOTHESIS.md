# exp_003 — Hypothesis: synthetic media matrix and multi-track determinism

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. The media generator produces all five standard profiles from
   `PERFORMANCE_TARGETS.md` (720p30, 1080p30, 1080p60, 1440p60, 4K30) with
   the exact frame byte size `width * height * 3 / 2` for I420.
2. It produces four independent WAV tracks with base tones 440, 880, 1320 and
   1760 Hz, each with a valid RIFF/WAVE header and expected data size.
3. Repeated runs produce byte-identical files (deterministic generator).

## Why

Latency, fidelity and A/V-sync benchmarks in `exp_000` and Phase 6 require
deterministic input across the full scenario matrix, and multi-audio requires
multiple distinguishable tracks. Without verifying the generator itself, a
later measurement could be contaminated by non-deterministic input.

## What would prove us wrong

- Missing profiles or incorrect I420 file sizes.
- Invalid WAV headers or wrong data chunk sizes.
- Non-identical hashes across two runs.

## Method

- `wormhole-harness media --config harness/config/example.toml` twice into
  separate output directories using two small TOML configs.
- Check file sizes against expected formulas, parse WAV headers, and compare
  SHA-256 values for every generated file.
- `cargo test --workspace --all-targets` for the profile-matrix unit test.

## Notes

This validates only the harness generator, not any codec or network path. No
performance claim is made here.
