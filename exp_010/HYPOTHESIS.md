# exp_010 — Hypothesis: all-C media generation and reference peers

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. The C harness generates deterministic I420 video (main + 5 profiles) and
   four WAV tracks.
2. The C reference sender, C proxy and C reference receiver interoperate:
   8 frames produce 16 packets (8 JPEG + 8 WAVE), all JPEGs decode at
   1280x720, and the capture round-trips byte-identically through the C
   dissector.
3. MSVC `/W4 /WX` compiles all C sources, including vendored JPEG
   encode/decode headers, with zero warnings.

## Why

These were the last harness commands still implemented only in Rust. Verifying
them in C allows the Rust workspace to be removed, satisfying the user's
"all in C" directive across the whole repository.

## What would prove us wrong

- Build failures or warnings.
- Missing/undecodable frames, or a failed round-trip.
- Reference receiver failing to write its CSV summary.

## Method

- `cmd /c scripts\build_c.bat`
- C harness `media --config harness/config/c-reference.toml`
- Start C `ref-receiver` and C `proxy`, run C `ref-sender`, then `dissect`
  the client→target capture.

## Notes

An informal implementation run occurred before this file was written; the
formal run below is the cited evidence.
