# exp_006 — Hypothesis: two portable OBS instances stream through the harness proxy

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. Two self-contained portable OBS copies under `third_party/obs-portable/`
   can be launched simultaneously with `--portable --profile Wormhole
   --collection ...`; the Teleport plugin loads in both (log line
   `[obs-teleport] version: 0.8.0`).
2. With the sender configured for TCP port 18081 and the receiver pointed at
   the proxy's listen address 18080, the receiver connects and the proxy
   captures a live stream containing both `JPEG` and `WAVE` packets.
3. CPU usage of both OBS processes can be sampled while streaming, and the
   captured JPEG frames decode successfully at 1280x720.

## Why

This is the first end-to-end observation of real Teleport behavior, replacing
the source-derived and synthetic-fixture evidence with a live capture. It
also validates the two-instance monitoring setup that later CPU/latency/
quality benchmarks will use.

## What would prove us wrong

- OBS instances fail to start, or the plugin does not load.
- No TCP connection / no packets captured.
- Captured packets are not decodable, or the dissector round-trip fails.
- CPU sampling yields no data.

## Method

- `scripts/setup_portable_obs.ps1` configures both copies.
- Start `wormhole-harness proxy --config harness/config/obs-proxy.toml`.
- Start receiver OBS then sender OBS with the portable flags.
- Wait ~8 s; sample per-process CPU with `Get-Process`/`GetProcessTimes`.
- Stop OBS and proxy; dissect `exp_results/obs-capture.bin`.

## Notes

No product performance claim is made until `exp_000` is complete; this
experiment establishes that measurement is possible and records raw numbers.
