# exp_007 — Hypothesis: all-C harness reproduces capture analysis

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. `scripts/build_c.bat` compiles the C harness (`check`, `synth`, `dissect`)
   with MSVC `/W4 /WX` with zero warnings, alongside the C protocol tests.
2. The C `dissect` command on the live OBS capture reports the same counts as
   the Rust dissector — 768 packets, 300 JPEG, 468 WAVE, 8,389,332 bytes — and
   verifies byte-identical re-encoding.
3. `synth` followed by `dissect` works entirely in C on a deterministic
   fixture.

## Why

The user requested all in C. Logging a decision is not enough: the harness
must actually run in C before the Rust implementation can be removed. This
experiment verifies that the C harness handles the same real capture as the
Rust one.

## What would prove us wrong

- MSVC warnings/errors.
- Different packet/byte counts or a failed round-trip against the live capture.
- `synth`/`dissect` failing in C.

## Method

- `cmd /c scripts\build_c.bat`
- C harness `--config harness/config/obs-proxy.toml dissect`
- C harness `--config harness/config/example.toml synth` then `dissect`

## Notes

An informal implementation run occurred before this file was written; its
output is superseded by the formal run recorded in `FINDINGS.md`.
