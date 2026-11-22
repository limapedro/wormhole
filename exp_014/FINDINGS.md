# exp_014 — Findings: real Wormhole plugin replaces Teleport on both OBS instances

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Evidence:** `exp_results/wormhole-only.jsonl` (MCP responses),
`exp_results/wormhole-libobs-matrix.png` and the user-confirmed follow-up screenshot,
OBS logs `third_party/obs-portable/{sender,receiver}/config/obs-studio/logs/2026-11-22 05-09-*.txt`.

## Verified results

- **Plugin builds and loads.** `wormhole.dll` compiled with MSVC `/W4 /WX`
  against OBS 33.0.0 headers, SIMD libjpeg-turbo 3.2.0 and an import library
  generated from `obs.dll` / `obs-frontend-api.dll`. Both OBS logs:
  `[wormhole] version 0.2.0`, `wormhole.dll` in loaded modules.
- **Tools menu.** Both logs show `[wormhole] Tools menu installed`. The
  settings dialog exposes only `Wormhole Enabled`, `TCP Port` and
  `JPEG Quality` — the `Audio Tracks` field was removed per the user's
  directive.
- **Teleport removed.** `obs-teleport.dll` absent from both portable plugin
  directories; no `obs-teleport` lines in either log.
- **Sender configured.** Primary monitor Display Capture (`monitor = 0`,
  `capture_cursor = true`) plus `Desktop Audio 2`, `Mic One`, `Mic Two`.
  Sender output starts on TCP 18090 from the Tools-driven settings. Log:
  `listening on 18090`, `output enabled via control source`, `receiver
  connected`, `v2 negotiated`, `first raw video callback`.
- **Receiver configured.** One `wormhole-source` named `Wormhole` in the scene,
  connected to 127.0.0.1:18090. Both OBS previews show the same desktop with
  correct colors (confirmed visually by the user).
- **Multi-audio follows OBS.** OBS's own `obs_output_set_mixers()` with
  `MAX_AUDIO_MIXES` drives `raw_audio2`; Wormhole emits STREAM_DESC frames
  dynamically for tracks 1–6 as OBS delivers them (receiver log shows
  `track 1 … track 6`, 48 kHz, 2 ch, 16-bit, shared clock 1). No user setting.
- **Stability.** Streaming ran for tens of seconds at 30/60 fps with CPU
  ~1.5–3 % per instance; no crash after the fixes below.

## Bugs found and fixed (negative results retained)

1. 4 MB stack-allocated frame buffers on 1 MB worker threads → stack overflow
   (`c00000fd`). Moved to heap allocations.
2. `sizeof(frame)` became 8 after the heap change, silently truncating v2
   frame builders — fixed by passing the real 4 MB size.
3. Audio items allocated with OBS `bzalloc` but freed with CRT `free()` →
   heap corruption (`0xC0000374`). Fixed to `bfree`.
4. CRT mismatch: turbojpeg built with `/MT` while the plugin uses `/MD`.
   Rebuilt with `WITH_CRT_DLL=TRUE` and added explicit `/MD`.
5. Receiver used absolute OBS timestamps → black preview. Now subtracts a
   stream offset like Teleport.
6. Incorrect color matrix/ranges produced a green tint. Fixed by calling
   libobs `video_format_get_parameters(VIDEO_CS_709, VIDEO_RANGE_PARTIAL, …)`.
7. Tools-menu source `wormhole-control` was a FILTER with
   `OBS_SOURCE_CAP_DISABLED`, so OBS WebSocket's `CreateInput` rejected it.
   Changed to a plain INPUT.
8. OBS didn't call `raw_video` until `obs_output_begin_data_capture()` was
   invoked after a client connected — added.

## Consequences

The plugin is functional end to end on Windows with two real OBS instances.
Still to do: settings persistence verification via the Tools dialog, Linux and
macOS builds, installer, documentation updates, and CI coverage of the plugin
build (currently only the C harness is in CI because the OBS SDK is generated
locally).
