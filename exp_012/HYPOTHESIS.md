# exp_012 — Hypothesis: real 60-second 1080p60 NVENC H.264 recording on the streaming PC

**Status:** hypothesis written before the experiment.

## What we expect

1. The gaming PC's Display Capture (`monitor_capture`, monitor 0 / primary
   2560x1440 screen) is present, enabled and actively rendering for the whole
   session.
2. The streaming PC records the received Teleport stream for **60 seconds**
   using Advanced output: NVENC H.264, 1920x1080, 60 fps, 8000 kbit/s, P4
   preset, MP4 container.
3. The resulting file exists, is ~60 s long, 1920x1080, 60 fps, H.264, and
   its average video bitrate is near 8 Mbit/s.
4. CPU/memory/frame stats are gathered from both OBS instances during the run.

## What would prove us wrong

- Recording fails to start, or the encoder is not NVENC H.264 / not P4 / not
  8000 kbit/s.
- File duration materially differs from 60 s, resolution/fps are wrong, or no
  file is produced.
- Display Capture is not active on the sender.

## Method

- Set receiver profile `Output/Mode=Advanced` and `AdvOut` recording keys
  (encoder, format, path, rescale, `recordEncoder.json`).
- Launch both instances through OBS-MCP with the 40 Mbit/s proxy running.
- Verify sender source settings via `GetInputSettings` / `GetSourceActive`.
- `StartRecord` on the receiver, wait 60 s, `StopRecord`.
- Inspect the file with ffprobe (or OBS logs if ffprobe is unavailable), plus
  `GetRecordStatus`, `GetStats`, CPU sampling and the wire capture.

## Notes

This is a real acceptance test of the two-PC simulation, not a Wormhole
performance claim. Large recordings are git-ignored; only measurements and
findings are committed.
