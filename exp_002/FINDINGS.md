# exp_002 — Findings: reference peer loopback fidelity

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Config:** `harness/config/reference-test.toml`

## What happened

Formal run: `ref-receiver` listened on `127.0.0.1:18081`; the TCP proxy listened
on `18080` and forwarded to `18081`; `ref-sender` sent 10 frames at 1280x720,
30 fps, JPEG quality 90.

- Reference receiver: **20 packets (10 video, 10 audio), 0 decode failures**.
- CSV rows: 20; `JPEG` 10, `WAVE` 10; 10 `decoded=ok` rows, 0 errors.
- 10 unique timestamps: first `0`, last `300,000,000 ns` (33333333 ns steps).
- Decoded JPEG dimensions: 1280 x 720.
- `dissect` on the proxy capture: **20 packets, 359,903 bytes, decode→encode
  byte-identical**.
- Proxy capture SHA-256: `5125EBC124649B47FC527F71EEE925AAEEC5CC9259C5801B1358A99A5BEB0254`.

Determinism check: two independent runs on different ports
(`exp_results/ref-det-{1,2}.toml`) produced captures with identical length
(359,903 bytes) and identical SHA-256 (`5125EBC1…0254`).

## What was learned

- All three hypotheses were **confirmed**. The standalone reference peers
  speak the documented wire format, produce valid JPEG that an independent
  decoder accepts, preserve timestamps and use the expected two-packet-per-
  frame structure.
- The reference peers are now usable as the differential-test endpoints.
- This is an independent check of framing: unlike `exp_001`, the byte stream
  came from real JPEG encoding rather than a hand-built fixture.
- Limitation: the peers implement the spec derived from Teleport source; they
  have **not** been tested against the real Teleport plugin. This experiment
  does not close the `C`-class capture items in PROTOCOL.md, and is not a
  performance measurement.

## Negative result (recorded)

A first attempt at the determinism check used string replacements to rewrite
the proxy config but failed to update the proxy's `listen` address, so the
sender was refused a connection and both capture files were empty (SHA-256 of
an empty file). The check was repeated with explicit, correct config files and
produced the identical hashes above. The failed attempt is retained here
rather than deleted.

## Consequences

- Issue #19 (reference sender/receiver) can be closed.
- Next: use these peers with the differential tester once Wormhole exists, and
  obtain a real Teleport 0.8.0 capture to confirm the `C`-class protocol facts.
