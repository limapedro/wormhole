# exp_010 — Findings: all-C media generation and reference peers

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- MSVC `/W4 /WX` compiled every C source, including vendored
  `stb_image.h` and `stb_image_write.h`, with zero warnings; all C protocol
  tests passed (`ALL C TESTS PASSED`).
- C `media` wrote the main I420 video, 5 resolution profiles and 4 WAV tracks
  to `exp_results/c-media`.
- C `ref-sender` → C `proxy` → C `ref-receiver`: sender reported
  `sent 8 frames`; receiver CSV had **16 rows, 8 `decoded=ok` JPEG frames**
  at 1280x720; C dissector on the captured client→target stream found
  **16 packets (8 JPEG + 8 WAVE), 182,865 bytes, byte-identical re-encode**.

## What was learned

All three hypotheses were confirmed. Every harness command is now implemented
in C: `check`, `synth`, `dissect`, `proxy`, `quality`, `differential`, `media`,
`ref-sender`, `ref-receiver`. The Rust workspace is no longer needed.

Negative results recorded during implementation:
- The reference sender initially exited silently because a 512-byte wire
  buffer could not hold a 1280x720 JPEG; fixed with a sized allocation.
- Winsock `WSAStartup` was missing from the reference peers (the proxy did it
  internally); moved to CLI startup.
- A second `sizeof(wire)` on the WAVE path (pointer size, not capacity) caused
  another silent failure; fixed.
- PowerShell-generated TOML files have a UTF-8 BOM that the C parser did not
  skip, so the first section was ignored; fixed by detecting and skipping the
  BOM.
- The C proxy did not propagate connection close, so the reference receiver
  never saw EOF; fixed with `shutdown()` on the opposite socket.

## Consequences

- The Rust workspace can now be removed from `main`, leaving a genuinely
  all-C repository. Historical experiments `exp_001`–`exp_009` remain as
  evidence; their Rust-specific details are documented as historical.
- Next: delete Rust sources, remove the Rust CI job and update all docs to
  describe the all-C commands.
