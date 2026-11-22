# exp_014 — Hypothesis: a real Wormhole plugin can replace Teleport on both OBS instances

**Status:** hypothesis recorded before the final evidence summary; development
runs preceded this file and are superseded by the verified state in FINDINGS.md.

## What we expect

1. `wormhole.dll` builds in C against OBS 33.0.0 and appears in the OBS **Tools**
   menu, like Teleport.
2. Teleport is absent from both portable OBS copies; no `obs-teleport` lines
   appear in their logs.
3. The gaming PC sends its primary 2560×1440 monitor downscaled to 1920×1080
   with three audio sources; the streaming PC receives and displays it.
4. Audio track count is not configured by the user — it follows OBS's own
   active mixer tracks (via `obs_output_set_mixers` / `raw_audio2`).
5. No OBS crash or freeze during streaming.

## What would prove us wrong

- Plugin fails to load, no Tools menu, Teleport still loads.
- Sender output never starts, receiver gets no frames, preview stays black or
  colors are wrong.
- Any crash (heap corruption, stack overflow).

## Method

- Build SIMD libjpeg-turbo 3.2.0 locally, generate `obs.lib` and
  `obs-frontend-api.lib` from installed DLL exports.
- Build `plugin/` with MSVC `/W4 /WX`.
- Reset both portable instances with `scripts/setup_portable_obs.ps1`
  (Teleport removed, Main Monitor + three audio sources on sender, one Wormhole
  source on receiver).
- Launch both through OBS-MCP, verify logs, source activity, and a desktop
  screenshot of both previews.
