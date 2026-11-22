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

## Channel dropdown (follow-up)

The sender advertises its `wormhole-audio-output` sources (`name|port` lines in
a `WH2_MSG_AUDIO_CHANNELS` frame) over the video connection. The receiver's
video source learns them together with the connected host, and the
`wormhole-audio` properties show a `> AUDIO SOURCE ON GAMING PC` combobox —
e.g. `TONE (127.0.0.1:18091)`. Selecting an entry fills host/port automatically,
so no manual port entry is needed. Verified with
`exp_results/wormhole-channels.jsonl`: the list property returned
`TONE  (127.0.0.1:18091)` and the recording had the tone at -25.5 dB.

## Simplified UX (follow-up)

- The gaming PC now auto-creates one `Wormhole Audio - <source>` sender per
  audio source in its Audio Mixer, on ports 18091+; no manual sender setup.
  References are held for the output lifetime (releasing them destroyed the
  sources immediately).
- The streaming PC's `Wormhole Video Capture` comes from the gaming PC's video
  output; `Wormhole Audio` sources pick a channel from a dropdown of the
  gaming PC's mixer sources (name from `Advanced Audio Properties`) with host
  and port filled automatically.
- Verified dropdown items: `Desktop Audio`, `Mic/Aux`, `Desktop Audio 2`,
  `Mic One`, `Mic Two`, `TONE`, each with its auto port.

## Clean test setup

`scripts/setup_portable_obs.ps1` now strips the global `DesktopAudioDevice*` /
`AuxAudioDevice*` entries from the scene template and disables them in
`basic.ini`, so the test profile contains only:

- Gaming PC: `Main Monitor` + `Desktop Audio 2` (one auto sender on port 18091).
- Streaming PC: `Wormhole Video Capture` + `Wormhole Audio` (port 18091).

Verified: sender `audio output 'Desktop Audio 2' connected`, receiver
`audio input name: Desktop Audio 2`, `learned 1 remote audio channel(s)`.
Both source lists contain only those entries.

## Limitations

- Each channel needs its own unique port; no automatic port negotiation.
- Discovery lists game PCs, not individual channels — pick the port manually.
- Receiver reconnects but sender audio outputs accept one client at a time.
