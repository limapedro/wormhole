# exp_018 — Findings: Wormhole v0.2 scene mirror, track audio and clean Tools UX

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Evidence:** `exp_results/wormhole-local-preserve2.jsonl`,
`exp_results/wormhole-control-sync.jsonl`, and OBS logs
`third_party/obs-portable/{sender,receiver}/config/obs-studio/logs/2026-11-22 07-08-*.txt`.

## Verified results

### One video signal + named mirror

- The composited `WH2_MSG_VIDEO` path is unchanged. The receiver's mirrored
  scene item is renamed to the sender's video source name:
  `[wormhole] video mirror named 'Main Monitor'`.
- Receiver `GetSceneItemList` for `WH Scene` returned `Main Monitor`,
  `Desktop Audio 2`, `Mic One`, `Mic Two` plus the local `STREAM-PC MIC` —
  matching the sender's source names in `exp_results/wormhole-local-preserve2.jsonl`.

### Audio = OBS output tracks

- Sender: `[wormhole] audio tracks 1-2 enabled`, then
  `audio stream 1: 'Desktop Audio'`, `audio stream 2: 'Mic/Aux'`.
- Receiver: `track 1: 'Desktop Audio' …`, `track 2: 'Mic/Aux' …`, and channel
  bindings from the mirrored scene:
  `audio channel 1 -> 'Desktop Audio 2'`, `audio channel 2 -> 'Mic One'`,
  `audio channel 3 -> 'Mic Two'`. Track IDs therefore map to the sender's
  source names on the receiver, keeping the UI consistent.

### Non-destructive scene mirroring (owner's constraint)

- Receiver log: `[wormhole] preserved 1 local source(s) on receiver`.
- `STREAM-PC MIC` (a local `wasapi_input_capture` added to the streaming PC's
  `Scene` before the sender connected) remained in the original `Scene` and was
  **also** copied into `WH Scene` with its transform.
- No receiver scene or source was deleted or renamed by sync; the original
  `Scene` still exists alongside `WH Scene`.

### Live state sync

- Muting `Mic One` on the sender produced a manifest change
  (`[wormhole] scene state updated (153 bytes)`); OBS WebSocket on the receiver
  returned `inputMuted=True` for `Mic One`. Because that source is the bound
  audio channel 2, the mute affects routed audio, not only the UI.
- The sender pushes a full state snapshot when it changes (versioned `V:1`
  manifest with `A:` active scene, `S:` scenes, `I:` name/kind/visible/track/muted),
  so scene switches, mute and visibility all propagate without a separate
  event protocol.

### Tools UX

- Two explicit entries: `Wormhole - Host Game PC...` (server settings: engage,
  port, audio tracks 1–6, JPEG quality, identifier, diagnostics) and
  `Wormhole - Mirror Game PC...` (opens/creates the receiver source properties).
- Log: `[wormhole] Tools menu installed (Host / Mirror)`.
- Both panels keep the cyberpunk styling and the receiver panel states that
  local mic/webcam sources are preserved.

### Correctness / regressions

- `build_c.bat` passes including the new `v2 text/scene-manifest round-trip`
  test; `build_plugin.bat` passes MSVC `/W4 /WX`.
- exp_016 invariants preserved: idempotent stop, `obs_output_end_data_capture()`
  on stop, `shutdown()` before close, receiver reconnect. No crash in testing.
- Protocol parser `wh2_parse_text()` now returns the parsed string length (0 on
  failure), which the unit test asserts.

## Notes / limitations

- The `T:` track table is informational; channels are bound by the order of
  audio items in the first scene. A scene with fewer audio items than configured
  tracks leaves trailing channels unbound.
- Sync is one-way (gaming PC authoritative). The receiver's `REQUEST SYNC`
  button still reconnects, which re-sends the manifest.
- No authentication; LAN only.
- Scene collection state is copied into new `WH ` scenes rather than replacing
  the receiver's collection, by design.
