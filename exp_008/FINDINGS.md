# exp_008 — Findings: all-C proxy captures the live OBS stream

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- `build\c\wormhole-harness.exe` (C, MSVC `/W4 /WX`, linked with `ws2_32.lib`)
  ran as the proxy for the two portable OBS instances via
  `scripts/run_two_obs.ps1 -Harness build\c\wormhole-harness.exe`.
- CPU over 6.03 s (12 logical CPUs): receiver 0.594 s (0.82 % of machine),
  sender 0.844 s (1.17 %); working sets 262.5 MB / 280.2 MB.
- C proxy capture: **8,632,459 bytes**. C dissector: **790 packets
  (309 JPEG, 481 WAVE, 0 other), byte-identical re-encode**.
- Image quality on that C-captured stream: 309 decoded 1280x720 frames, mean
  RGB (18,1,230), MSE 0.000, PSNR capped 120 dB.

## What was learned

All three hypotheses were confirmed. The C implementation now covers the full
capture path: proxy → capture → dissect, using a real Teleport 0.8.0 stream
between two OBS instances. Packet counts differ from `exp_006` (768 vs 790)
because run duration and OBS timing vary; the structural criteria (JPEG+WAVE,
byte identity, decodable frames) hold.

Negative result: MSVC `/WX` rejected an implicit `uint64_t → unsigned long`
conversion in the bandwidth-cap sleep. Fixed by computing the delay in
`uint64_t`, clamping to one second, then casting explicitly.

## Consequences

- Remaining Rust-only harness commands: `quality`, `differential`, `media`,
  `ref-sender`/`ref-receiver`. These are temporary; the primary OBS monitoring
  workflow is now all C except the quality report command.
- Next: port quality analysis to C (requires a JPEG decoder) or vendor a
  decoder, then remove the Rust harness from the default workflow.
