# exp_011 — Hypothesis: gaming PC → streaming PC workflow over a 40 Mbit/s link

**Status:** hypothesis written before the experiment.

## Setup

- **Gaming PC** (portable OBS sender): 2560x1440 base (matches the physical
  display), downscaled to 1920x1080 output at 60 fps. Sources: Display Capture
  of the primary screen plus three audio inputs (desktop audio and two
  capture devices). Teleport output enabled on TCP 18081.
- **Streaming PC** (portable OBS receiver): 1920x1080/60, Teleport source
  connected through the harness proxy at 127.0.0.1:18080.
- **Network**: proxy bandwidth cap 40 Mbit/s = 5,000,000 bytes/s in both
  directions, with lossless capture of sender→receiver traffic.

## What we expect

1. Both instances launch with zero Plugin Errors dialogs, their WebSocket v5
   endpoints answer `GetVersion`, and `GetVideoSettings` reports
   2560x1440 → 1920x1080 (sender) and 1920x1080 (receiver).
2. OBS WebSocket confirms a Display Capture input and three audio inputs on
   the sender; `GetStats` reports non-zero frames and audio traffic on both.
3. The proxy capture contains JPEG + WAVE packets at ~1080p, decodable with
   measurable fidelity; observed bandwidth stays near but below 40 Mbit/s.
4. CPU and working set can be sampled for both processes during the stream.

## What would prove us wrong

- Sources cannot be created, or OBS reports no frames/audio.
- Capture is empty, undecodable, or bandwidth materially exceeds the cap.
- Dialogs block launch or WebSocket calls fail.

## Method

- Configure profiles (`BaseCX/CY`, `OutputCX/CY`, `FPSCommon`).
- Start the 40 Mbit/s proxy via OBS-MCP `capture_start`.
- Launch both instances via OBS-MCP `obs_launch`.
- Add sources and query state with `obs_ws_call` (GetVersion,
  GetVideoSettings, GetSceneList, GetInputList, GetStats, CreateInput).
- Sample `obs_cpu_stats`, tail logs, dissect and score the capture with the C
  harness (`dissect`, `quality`).
- Stop everything via MCP `capture_stop` and `obs_terminate`.

## Notes

This measures a **Teleport** baseline workflow on this machine; it is not a
Wormhole performance claim. The display capture contents are the live desktop,
so image quality is reported as decoded-frame statistics (dimensions, mean RGB,
uniformity/error) rather than PSNR against a known synthetic source.
