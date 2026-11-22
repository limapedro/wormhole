# exp_012 — Findings: real 60-second 1080p60 NVENC recording on the streaming PC

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Orchestration:** OBS-MCP (`build\obs-mcp.exe --config config/gaming-streaming.toml`), response log `exp_results/obs-record-60s-final.jsonl`.

## Configuration applied

Receiver profile `basic.ini`: `Output/Mode=Advanced`, `AdvOut/RecType=Standard`,
`RecEncoder=obs_nvenc_h264_tex`, `RecFormat2=mp4`, `RecFilePath` pointing at
`exp_results/recordings`, `RecRescaleRes=1920x1080`, `RecTracks=1`,
`RecAudioEncoder=aac` (160 kbit/s track 1). Encoder settings are in
`recordEncoder.json`:
`rate_control=CBR`, `bitrate=8000`, `max_bitrate=8000`, `preset=p4`,
`profile=high`, `tune=hq`, `multipass=qres`.

Gaming PC: existing Display Capture source (`monitor_capture`) verified by
`GetInputSettings` as `{"inputKind":"monitor_capture","inputSettings":{"capture_cursor":true,"monitor":0}}`
— the primary 2560×1440 monitor — and `GetSourceActive` returned
`videoActive=true, videoShowing=true`.

## What happened

- Both instances launched through OBS-MCP; `GetVideoSettings` confirmed sender
  **2560×1440 → 1920×1080 @ 60 fps** and receiver **1920×1080 @ 60 fps**.
- `StartRecord` succeeded; six 10-second waits produced a ~60 s window;
  `GetRecordStatus` just before stopping reported `outputDuration=59749 ms`
  (59.749 s) and `outputBytes=61,007,534`; `StopRecord` returned
  `exp_results/recordings/2026-11-22 03-51-46.mp4`.
- `ffprobe` on the final file:

| Property | Value |
|---|---|
| Duration | **59.983 s** |
| Video codec | **h264**, profile **High** |
| Resolution | **1920×1080** |
| Frame rate | **60/1** |
| Video bitrate | **8,009,512 bit/s** |
| Audio codec | aac LC, **159,888 bit/s** |
| Total bitrate | 8,181,126 bit/s |
| File size | 61,341,403 bytes |

- OBS log confirms the encoder actually used:
  `[obs-nvenc: 'advanced_video_recording'] settings: codec: H264,
  rate_control: CBR, bitrate: 8000, preset: p4, profile: high,
  width: 1920, height: 1080`, followed by `Recording Start` at 03:51:46.708 and
  `Recording Stop` at 03:52:46.935 (**60.2 s wall clock**).
- CPU during/around the recording: sender 0.953 s (3.97 %) before start and
  1.312 s (5.47 %) after stop; receiver 0.438 s (1.82 %) before and 1.188 s
  (4.95 %) after; working sets 363.5–388.3 MB sender, 297.3–338.7 MB receiver.
  Receiver `GetStats`: 60 fps, CPU 4.42 %, 425.7 MB.

## Negative results (recorded)

- Two earlier takes were 57.93 s and 63.97 s because CPU sampling was inside
  the wait window; moving all sampling outside the record window produced
  59.983 s.
- The first `GetInputSettings` attempt used `sourceName` and OBS rejected it
  (code 300); the correct field is `inputName`, used in the final run.
- The proxy capture for the final run was empty (proxy startup race); wire
  bandwidth evidence remains from `exp_011`, not re-claimed here.
- The recording file itself is large and git-ignored; only these measurements
  are committed.

## What was learned

All hypotheses were confirmed. A real, non-synthetic two-PC workflow produced
a compliant ~60 s 1080p60 MP4 recorded with NVENC H.264 CBR 8000 kbit/s P4 on
the streaming PC, while the gaming PC streamed its primary 2560×1440 monitor
downscaled to 1080p60 through a 40 Mbit/s link.

## Consequences

- Acceptance test for the simulated gaming/streaming setup is met.
- Remaining gap for latency/quality claims: a synthetic frame counter on the
  display capture source (current content is the live desktop, so PSNR against
  a reference is not available).
