# exp_001 — Findings: harness decode/proxy fidelity

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Configs:** `harness/config/example.toml`, `proxy-test.toml`, `proxy-dissect.toml`

## What happened

Formal run (after the hypothesis was written):

1. `wormhole-harness synth --config example.toml` generated 60 frames of
   alternating `JPEG` and `WAVE` packets: **120 packets, 184,800 bytes** in
   `exp_results/capture.bin`.
2. `wormhole-harness dissect --config example.toml` decoded all 120 packets and
   re-encoded them: **round-trip byte-identical**. JSON summary written to
   `exp_results/dissection.json`.
3. `sink` (port 18081) and `proxy` (listen 18080 → target 18081) were started;
   `replay` sent the fixture through the proxy.
4. `wormhole-harness dissect --config proxy-dissect.toml` decoded the proxy's
   capture: **120 packets, 184,800 bytes, round-trip byte-identical**.
5. `cargo test --workspace --all-targets`: **15 tests passed**
   (10 protocol, 5 harness). Includes truncated-input sweep, negative and
   oversized payload rejection, and a deterministic 2,048-length random-byte
   sweep that must not panic.

## Raw numbers

| File | Bytes | SHA-256 |
|---|---|---|
| `exp_results/capture.bin` (fixture) | 184,800 | `75487ADA5E197E207E71FE98D70AB5E1195EEB1026C77DAC306628E193764DDC` |
| `exp_results/proxy-capture.bin` | 184,800 | `75487ADA5E197E207E71FE98D70AB5E1195EEB1026C77DAC306628E193764DDC` |
| `exp_results/sink.bin` | 184,800 | `75487ADA5E197E207E71FE98D70AB5E1195EEB1026C77DAC306628E193764DDC` |

Dissector self-benchmark (informal, implementation smoke test — **not** a
Teleport or Wormhole performance result, and not cited by any performance
claim): 1,000 synthetic packets / 4,200,000 bytes decoded in 0.003 s
(≈295,928 packets/s), round-trip identical. Recorded in
`exp_results/dissector-bench.csv` (git-ignored).

## What was learned

- All three hypotheses were **confirmed**: the decode/re-encode path is
  byte-identical, the unimpaired proxy is lossless, and malformed input is
  rejected without panicking.
- The harness is now a trustworthy observation path for later capture work.
- Negative result from implementation: the first proxy test produced an empty
  capture because `replay` connected to `proxy.target` instead of
  `proxy.listen`. Fixed by making replay always feed the proxy's listen socket;
  the retest produced identical hashes. This is recorded rather than hidden.
- Limitation: this experiment uses a **synthetic** fixture generated from the
  same implementation that decodes it. It does not confirm the protocol
  against a real Teleport sender (`C`-class items in PROTOCOL.md remain open).

## Consequences

- Dissector and proxy can be used as building blocks for capture verification
  and the differential tester.
- Next experiment should capture real Teleport 0.8.0 traffic through this proxy
  and compare the decoded fields against the source-derived spec in
  PROTOCOL.md (`exp_002`).
