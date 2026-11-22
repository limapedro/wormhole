# exp_008 — Hypothesis: all-C proxy captures the live OBS stream

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. `build/c/wormhole-harness.exe proxy --config harness/config/obs-proxy.toml`
   links on Windows (ws2_32) and accepts the Teleport receiver's TCP
   connection, forwarding it to the sender's port 18081.
2. The C proxy records the sender→receiver direction to `obs-capture.bin`
   with the same packet structure as the Rust proxy (JPEG + WAVE packets).
3. The C dissector verifies byte-identical re-encoding of that C-captured
   stream.

## Why

The user asked for an all-C implementation and a harness sophisticated enough
to monitor two OBS instances. Porting the proxy is the last major capture-path
component still in Rust; verifying it against the real Teleport stream proves
the C path is complete end to end.

## What would prove us wrong

- Link or runtime failure of the C proxy.
- Empty or truncated capture, or no JPEG/WAVE packets.
- C dissector rejecting the C-captured stream.

## Method

- `scripts/run_two_obs.ps1 -Harness 'build\c\wormhole-harness.exe'`
- C harness `dissect --config harness/config/obs-proxy.toml`
- Compare counts with `exp_006` (768 packets / 8,389,332 bytes; run-to-run
  packet counts vary with timing, so structure and byte identity are the
  criteria).

## Notes

An informal compile check occurred before this file was written; the formal
run below is the cited evidence.
