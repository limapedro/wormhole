# exp_009 — Findings: all-C quality and differential commands

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- `cmd /c scripts\build_c.bat` compiled the C harness with the vendored
  public-domain `stb_image.h` JPEG decoder under MSVC `/W4 /WX` with zero
  warnings; all C protocol tests passed.
- C `quality` on the live OBS capture: **309 frames, 1280x720, mean RGB
  (18.0, 1.0, 230.0), MSE 0.000, PSNR 120.00 dB** (capped). The Rust
  implementation, run afterwards on the same capture, produced identical
  numbers.
- C `differential` comparing the capture with itself: **790 vs 790 packets,
  790 compared, 0 differences**, JSON report written.

## What was learned

All three hypotheses were confirmed. The C harness now covers the complete
analysis path: `check`, `synth`, `dissect`, `proxy`, `quality` and
`differential`. Only `media` generation and the reference sender/receiver
remain in Rust and are not needed for the OBS monitoring workflow.

No negative results in the formal run. An implementation detail worth
recording: the vendored decoder is compiled directly into the C harness with
`STB_IMAGE_IMPLEMENTATION`; its license note is in `vendor/stb/LICENSE.txt`.

## Consequences

- The two-OBS monitoring workflow can now run entirely with C tools except
  for the PowerShell orchestration script (which only launches processes and
  samples CPU).
- Next: port `media` and the reference peers to C, then remove the Rust
  workspace from `main` so the repository is genuinely all C.
