# exp_017 — Findings: per-source audio exploration (superseded by exp_018)

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Status:** exploratory; the source-based audio model was tried and is retained
only as history. The accepted model is output-track based — see
[exp_018/FINDINGS.md](../exp_018/FINDINGS.md).

## What was tried

- Sender bound every audio source with `obs_source_add_audio_capture_callback()`
  and sent one named stream per source. Receiver created `WH <source>` clones.
- A line-based scene manifest was added and sent after negotiation.

## Verified results

- Five sources were bound and logged with real names:
  `Desktop Audio`, `Mic/Aux`, `Desktop Audio 2`, `Mic One`, `Mic Two`.
- Only three emitted callbacks (`Mic/Aux`, `Mic One`, `Mic Two`); the two WASAPI
  output-capture sources never fired in the test setup. OBS 33's monitoring
  dedup (`obs-audio.c:507–546`) silences monitored sources at mix time but does
  not skip capture callbacks — the exact cause of the missing two callbacks was
  not established. This is why exp_018 uses `raw_audio2` mixer tracks instead.
- Three audio clones were created on the receiver from stream descriptors.

## Negative result: manifest emitter bug

The sender logged a 76-byte manifest containing `SScene`,
`IMain Monitorvideo`, `IDesktop Audio 2audio`, … — the `':'` separator after
the line prefix was never written, so the receiver's `S:` / `I:` parser matched
zero lines. `GetSceneList` showed only `Scene` on both machines and no scene was
created from the manifest. The bug was fixed in exp_018 by writing the colon
and replacing the format with a structured field list.

## Consequence

Per-source capture stayed as a research artifact; it is not the default audio
model. The clone-creation and `obs_queue_task(OBS_TASK_UI, …)` patterns were
retained and reused by exp_018.
