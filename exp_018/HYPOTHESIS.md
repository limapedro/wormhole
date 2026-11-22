# exp_018 — Hypothesis: one video signal, output-track audio channels and a mirrored scene UI (v0.2)

**Date:** 2026-11-22
**Status:** hypothesis recorded before implementation.

## What we expect

1. **One video signal.** The existing composited JPEG feed remains the only
   video stream; no per-source video capture is added.
2. **Audio = OBS output tracks.** The sender captures mixer tracks with
   `raw_audio2` / `obs_output_set_mixers()` (default tracks 1–2, configurable
   1–6) and sends each track as its own v2 network channel. Track names come
   from OBS's own `[AdvOut] Track<N>Name` profile values, read via
   `obs_frontend_get_profile_config()`. This matches "the output audios of the
   gaming PC" and avoids the per-source callback gap observed in exp_017 where
   two of five sources never fired.
3. **Scene mirror.** A versioned, structured scene manifest (scenes, item names,
   kinds, visibility, active scene, track table) is sent on connect and on
   request; the receiver recreates the sender's scene layout and adds one
   `wormhole-audio` clone per track.
4. **Live control.** Incremental control events propagate scene switches,
   source mute and item visibility from the gaming PC to the streaming PC
   within ~1 s.
4b. **Non-destructive merge (owner's late constraint).** The streaming PC may
   keep its **own microphone and webcam** as local sources while Wormhole only
   supplies gameplay video and game audio. Mirroring must therefore be
   **additive**: the receiver's existing scenes and sources are never deleted
   or modified; mirrored scenes are created with a `WH ` prefix, and local
   sources from the receiver's pre-sync scene are copied into each mirrored
   scene with their transforms. Mute/visibility events apply only to
   Wormhole-owned clones, never to local sources.
5. **UI/UX.** Pure-C cyberpunk settings panels with a real `REQUEST SYNC`
   action; no manual source placement on the streaming PC.
6. All exp_016 invariants hold: single stop on disable, `obs_video_active()=0`
   after disable, receiver reconnect, no OBS-thread blocking.

## What would prove us wrong

- Only one audio channel arrives or track names are missing.
- Receiver scene items do not mirror the sender after sync.
- Mute/hide/scene-switch events do not appear on the receiver.
- Any crash, double stop, or stuck Output Mode.

## Evidence to capture

- `build_c.bat` and `build_plugin.bat` output.
- Sender/receiver OBS logs with manifest, track and control lines.
- OBS WebSocket `GetSceneList`, `GetSceneItemList`, `GetInputList`,
  `GetInputMute`, `GetProfileParameter` responses.
- Screenshots of both OBS previews.
