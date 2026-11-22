# exp_004 — Findings: differential capture comparison

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## What happened

- A 4-frame synthetic capture (8 packets: 4 JPEG + 4 WAVE) was copied to two
  paths. `differential` reported `identical=true`, 8 vs 8 packets, 0
  differences, exit code 0.
- One byte in the first packet's timestamp was flipped in the right-hand copy.
  `differential` reported `identical=false`, 8 vs 8 packets, 1 difference,
  JSON entry `packet 0: timestamp 0 vs 1`, and exit code 1.
- `cargo test --workspace --all-targets`: 19 tests pass (10 protocol, 9
  harness). fmt/clippy clean.

## What was learned

All three hypotheses were confirmed: the comparison stage accepts identical
streams, detects a controlled single-byte timestamp change at the correct
packet index, and writes a complete JSON report. It is suitable as a CI gate.

Limitation: this stage compares **captures**, not live senders. It has not
compared original Teleport against Wormhole, and no parity claim follows from
this experiment. The live-driving half of the differential tester remains
pending.

## Consequences

- The capture-comparison stage of issue #21 is implemented; the live-sender
  half remains open.
- Future differential runs can use this command with one capture from the real
  Teleport sender (once obtainable) and one from Wormhole.
