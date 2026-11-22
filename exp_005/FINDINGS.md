# exp_005 — Findings: Visual Studio all-C core and wire compatibility

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- `cmd /c scripts\build_c.bat` used `vswhere` to locate Visual Studio Community
  2026 (`C:\Program Files\Microsoft Visual Studio\18\Community`), initialized
  `vcvars64.bat`, and compiled `core/c/protocol.c` plus the C tests with
  `cl /nologo /W4 /WX /Od /Zi /Iinclude`. Both targets compiled with **zero
  warnings**.
- All four C test groups passed: header layout, round-trip of JPEG/WAVE/
  unknown, malformed input rejection, and helper/multi-packet decoding.
  Output ends with `ALL C TESTS PASSED`.
- `build\c\capture_writer.exe exp_results\c-wire.bin 4` wrote 4 frames
  (8 packets, 640 bytes) using only the C encoder.
- The existing Rust harness dissector decoded all 8 packets and re-encoded
  them **byte-identically**. JSON summary: `packet_count=8`, `byte_count=640`,
  `round_trip_ok=true`. Capture SHA-256
  `C782F8EE587A45C709E49684875187866B16AC95BCD28B4AE28F56D3632E9281`.
- `cargo test --workspace --all-targets`: 19 tests pass.

## What was learned

- All three hypotheses were **confirmed**. Visual Studio can build the all-C
  core with warnings-as-errors, the C library implements the documented wire
  format correctly, and the C encoder's output is accepted byte-for-byte by
  the independent Rust dissector.
- The user's "all in C" directive is now backed by a working C implementation
  rather than just a decision log.
- One negative result: the first formal run failed because MSVC `/WX` rejected
  `fopen` as deprecated in `capture_writer.c`. Fixed by defining
  `_CRT_SECURE_NO_WARNINGS` for MSVC; subsequent run clean. Recorded rather
  than hidden.

## Consequences

- The all-C pivot in ARCHITECTURE_LOG.md is now evidence-backed.
- Next: port the harness commands to C (remove the Rust dependency from
  `main`), add the MSVC build to CI, and build the two-portable-OBS monitoring
  setup for CPU/latency/image-quality measurement.
