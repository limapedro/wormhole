# exp_007 — Findings: all-C harness reproduces capture analysis

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- `cmd /c scripts\build_c.bat` located Visual Studio 2026, compiled the C
  protocol library, tests, `capture_writer` and the new C harness with
  `cl /W4 /WX` — **zero warnings** — and printed `ALL C TESTS PASSED`.
- C harness on the live OBS capture:
  `dissect: 768 packets (300 JPEG, 468 WAVE, 0 other), 8389332 bytes, byte-identical`.
- C harness `synth` then `dissect` on its own fixture:
  `120 packets (60 JPEG, 60 WAVE), 9600 bytes, byte-identical`.
- The Rust dissector was run last on the same live capture and produced the
  same totals: 768 packets, 8,389,332 bytes, round-trip byte-identical.

## What was learned

All three hypotheses were confirmed. The C harness now performs the core
capture work (`check`, `synth`, `dissect`) with results identical to the Rust
implementation on real Teleport traffic. This is concrete progress on the
user's all-C directive: the product core and the primary harness path are C.

Negative results recorded: an informal run first reported a bogus round-trip
mismatch (`length 8389332 vs 8224`). Cause: `wh_encode` reports bytes for one
packet, but the dissector treated that value as cumulative; fixed by using a
local `encoded` count and adding it to `round_trip_len`. Also, the temporary
JSON from the earlier failed run was stale — the formal run above rewrote it.

## Consequences

- The C harness is usable now for capture verification. Still Rust-only and
  pending C ports: TCP proxy, image-quality analysis, differential comparison,
  media generation and the reference peers. These are explicitly temporary.
- Next step: port the proxy to C (Winsock/POSIX sockets) and remove the Rust
  implementation from the default workflow once parity is demonstrated.
