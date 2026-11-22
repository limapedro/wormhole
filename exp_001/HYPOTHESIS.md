# exp_001 — Hypothesis: harness decode/proxy fidelity

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. A deterministic synthetic capture of alternating `JPEG` and `WAVE`
   Teleport-compatible packets can be decoded by `wh-protocol` and
   re-encoded **byte-identically** (dissector round-trip).
2. The TCP capture/impairment proxy, with no impairments configured, preserves
   the sender-to-receiver byte stream exactly: the sink receives a file with
   the same SHA-256 as the source fixture, and the proxy's own capture is
   byte-identical to both.
3. The decoder rejects malformed input (negative size, truncated header,
   oversized payload) without panicking.

## Why

Before the harness can be trusted to reverse-engineer or differentially test
Teleport, its own observation path must be proven lossless. If the proxy or
dissector alters bytes, every later protocol claim and benchmark would be
unreliable. This is the first correctness gate of Phase 2.

## What would prove us wrong

- A SHA-256 mismatch between fixture, proxy capture and sink file.
- Any decode error on the deterministic fixture.
- A decode→encode round-trip difference (we would not be able to validate a
  real capture).
- A panic or hang on the malformed-input tests.

## Method

- `wormhole-harness synth` → `exp_results/capture.bin` (60 frames, 120 packets).
- `wormhole-harness dissect` → JSON summary + round-trip assertion.
- `wormhole-harness sink` + `wormhole-harness proxy` +
  `wormhole-harness replay` → `exp_results/sink.bin` and
  `exp_results/proxy-capture.bin`.
- Compare SHA-256 of all three files.
- `cargo test --workspace` covers malformed input and fuzz-ish byte sweeps.

## Notes

An informal smoke run of these commands occurred during implementation; its
results are superseded by the formal run recorded in `FINDINGS.md`. Only the
formal run is cited as evidence.
