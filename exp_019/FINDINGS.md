# exp_019 — Findings: per-channel audio sources with one thread and connection each

**Date:** 2026-11-22
**Evidence:** `exp_results/wormhole-perchannel.jsonl`, OBS logs
`third_party/obs-portable/{sender,receiver}/config/obs-studio/logs/2026-11-22 08-15-*.txt`,
recording `C:\Users\multi\Videos\2026-11-22 08-19-20.mp4` analysed with ffmpeg.

## Design adopted (owner's direction)

- Video stays a single composited stream (`wormhole-source` -> `wormhole-output`).
- Audio is no longer multiplexed into the video connection:
  - `wormhole-audio-output` (gaming PC): pick any OBS audio source from a
    dropdown, choose a unique TCP port, own capture callback, bounded queue,
    worker thread and TCP listener.
  - `wormhole-audio` (streaming PC): host/port or discovered node, own reader
    thread and TCP connection, outputs one `obs_source_audio` stream.
- Add as many pairs as needed. The user assigns OBS recording tracks in
  Advanced Audio Properties, so each channel can be edited separately.

## Verified results

- Sender log: `[wormhole] audio output 'TONE' listening on 18091` then
  `audio output 'TONE' connected`.
- Receiver log: `audio input name: TONE` — descriptor received on the separate
  channel connection; video linked independently on port 18090.
- Both OBS instances remained alive; no crash.
- Recording of a 1 kHz test tone: peak -25.9 dB in the 950–1050 Hz band
  (source file peaks -21.1 dB) and **no silence gaps** from `silencedetect`.
- UI panels for both audio sources now have a cyberpunk banner, a short
  step-by-step guide, and (on the receiver) discovered-node selection.

## Bugs fixed during this experiment

- Sender `raw_video` crashed on NV12 frames because chroma plane 2 is NULL;
  copy is now NV12-aware and row-based with padded linesizes.
- Receiver crashed in `video_frame_init` because `ensure_yuv` zeroed the frame
  without setting `video_frame.width/height`; now set explicitly.
- Receiver validated decoded dimensions before `obs_source_output_video2`.
- Audio sources were destroyed immediately after creation because the only
  reference was released; the context now keeps the reference.
- Audio was inaudible in recordings because the sources were not in a scene;
  they are now added to the current scene (non-destructively).
- Periodic audio dropouts were caused by forwarding sender timestamps; using
  receive-clock timestamps (`os_gettime_ns()`) removed the gaps.

## Limitations

- Each channel needs its own unique port; no automatic port negotiation.
- Discovery lists game PCs, not individual channels — pick the port manually.
- Receiver reconnects but sender audio outputs accept one client at a time.
