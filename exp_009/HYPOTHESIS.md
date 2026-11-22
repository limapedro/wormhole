# exp_009 — Hypothesis: all-C quality and differential commands

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. The C harness compiles with the vendored public-domain `stb_image.h`
   decoder under MSVC `/W4 /WX` and POSIX `-Wall -Wextra -Werror`.
2. C `quality` on the live OBS capture reports the same values as the Rust
   command: 309 decoded 1280x720 frames, mean RGB (18,1,230), MSE 0.000,
   PSNR capped at 120 dB.
3. C `differential` reports zero differences when comparing the live capture
   with itself (790 packets, 790 compared).

## Why

These were the last two analysis commands still implemented only in Rust.
Verifying them in C completes the all-C harness analysis path and lets the
Rust dependency be removed from the default workflow.

## What would prove us wrong

- Compiler errors/warnings from the vendored decoder.
- Different frame counts, dimensions or quality metrics.
- False differences on identical captures.

## Method

- `cmd /c scripts\build_c.bat`
- C harness `quality --config harness/config/obs-quality.toml`
- C harness `differential --config exp_results/diff-c.toml`

## Notes

An informal implementation run occurred before this file was written; the
formal run below is the cited evidence.
