# exp_002 — Hypothesis: reference peer loopback fidelity

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. A standalone Rust reference sender speaking the source-derived Teleport
   wire format can stream real JPEG video and PCM audio through the harness
   proxy into a standalone reference receiver.
2. The receiver decodes every JPEG frame successfully at the configured
   resolution, records the original timestamps, and reports zero decode
   failures.
3. The proxy capture decodes and re-encodes byte-identically, confirming that
   the reference peers and the dissector agree on framing.

## Why

The differential tester (Phase 2) needs minimal standalone peers that speak
the original protocol. Before comparing Wormhole against real Teleport, these
peers must be proven to produce a valid, decodable stream. This also provides
an independent check of the framing rules in `PROTOCOL.md` that does not use
the synthetic fixture from `exp_001`.

## What would prove us wrong

- Any JPEG decode failure in the reference receiver.
- Missing or duplicated packets (expected: 2 packets per frame, 10 frames).
- Timestamp or resolution mismatch against the configured values.
- Non-byte-identical dissector round-trip on the proxy capture.

## Method

- `ref-receiver --config harness/config/reference-test.toml` listens on
  `127.0.0.1:18081`.
- `proxy --config harness/config/proxy-test.toml` listens on `18080` and
  forwards to `18081`, recording `exp_results/proxy-capture.bin`.
- `ref-sender --config harness/config/reference-test.toml` sends 10 frames at
  1280x720, 30 fps, JPEG quality 90.
- Inspect `exp_results/reference-receiver.csv` and run `dissect` on the proxy
  capture.

## Notes

An informal implementation smoke run occurred before this file was written;
its output is superseded by the formal run in `FINDINGS.md`. This experiment
does **not** involve the real Teleport plugin and makes no claim about
Teleport compatibility beyond the source-derived wire format.
