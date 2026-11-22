# exp_017 — Hypothesis: per-source audio routed over the network with automatic receiver clones

**Date:** 2026-11-22
**Status:** hypothesis recorded before implementation; results in FINDINGS.md.

## Problem

- Only one audio channel arrives at the receiver and it sounds like a mix.
- The gaming OBS instance has two audio sources (e.g. `Mic One`,
  `Desktop Audio 2`). Today Wormhole uses `raw_audio2` mixer tracks, which
  are **post-mix** — every source assigned to the same mixer is summed into
  one track before we ever see it. That is why there is effectively one
  channel.
- The user proposes: adding a Wormhole source should automatically create
  matching source clones on the receiver, and each source's audio should be
  routed independently over the network.

## Research findings (libobs 33.0.0 headers/source)

- `obs_source_add_audio_capture_callback(source, callback, param)` exists and
  delivers **pre-mix, per-source** `audio_data` (float planar, frames,
  timestamp, muted flag). It is mutex-protected and thread-safe.
- `obs_enum_sources()` can find every source; `OBS_SOURCE_AUDIO` output flag
  identifies audio-capable ones. `obs_source_get_name()` gives the real name.
- `obs_source_output_audio()` can push audio into a new source.
- `obs_queue_task(OBS_TASK_UI, ...)` allows creating sources and adding scene
  items from the worker thread safely.

## What we expect

1. Sender captures each audio source independently with its real name and
   assigns a unique `track_id` per source; STREAM_DESC carries the real name.
2. Receiver automatically creates a `Wormhole Audio` clone for each incoming
   source descriptor, adds it to the current scene, and routes only that
   source's audio to it.
3. Receiver's Sources list shows `Mic One`, `Desktop Audio 2` etc. as distinct
   sources with independent audio; no single mixed track.
4. Existing video path and Tools menu keep working; no crashes.

## What would prove us wrong

- Only one audio track still arrives; clone sources do not appear.
- OBS crashes on source creation from the worker thread.
- Audio is distorted/silent or track IDs collide.
