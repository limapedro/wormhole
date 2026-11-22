# exp_000 — Hypothesis: baseline measurement plan for original Teleport

**Status:** hypothesis written before experiment (per methodology). No
measurements have been run yet.

## What we expect

Original Teleport (Go-based `obs-teleport`) will serve as a measurable
baseline. We expect to observe:

1. A deterministic baseline for CPU, resident memory, latency, frame pacing,
   bandwidth, startup/reconnect and A/V sync on the dev machine for scenarios
   S1–S5 of [PERFORMANCE_TARGETS.md](../PERFORMANCE_TARGETS.md) (720p30,
   1080p30, 1080p60, 1440p60, 4K30).
2. CPU cost dominated by image compression (JPEG-family encode/decode is the
   working hypothesis) plus Go GC pauses and cgo boundary crossings; memory
   dominated by per-frame buffers and Go heap growth under load.
3. Glass-to-glass latency comfortably above one frame, because of buffering,
   compression and Go scheduling/allocation.
4. Bandwidth scaling with resolution and quality setting in a way we can fit
   with a simple model, giving a "quality setting → bitrate/PSNR" curve.
5. A/V sync that is adequate for a single audio track but whose mechanism will
   need to be understood precisely before adding multi-audio.

These are expectations, not facts. The point of `exp_000` is to replace them
with numbers.

## Why

The success definition in [GOAL.md](../GOAL.md) requires parity first and
measured efficiency second. Efficiency targets ("≥50% lower CPU", "≥50%
lower memory", "<10 ms sync") are meaningless until the Teleport baseline is
measured under identical, reproducible conditions. `exp_000` also produces
the first artifacts that exercise the harness and validate the synthetic
media generators.

## What would prove us wrong

- If measured baseline CPU/memory is already extremely low, the ≥50%
  improvement targets may be unrealistic and must be revised with evidence.
- If the compression/GC/cgo hypothesis is wrong (e.g. time is dominated by
  socket syscalls or OBS itself), optimization effort must be redirected.
- If latency is already near one frame, the "no additional latency" target
  needs a tighter definition.
- If the harness cannot produce deterministic input, latency/bandwidth
  numbers are not comparable and the methodology must be fixed first.

## Plan

1. Clone upstream into git-ignored `third_party/`, pin and record the exact
   commit hash in [PROTOCOL.md](../PROTOCOL.md).
2. Complete the license check before publishing any derived code (RISKS.md
   R01). Baseline measurement of unmodified upstream binaries is fine.
3. Install Go (missing locally — RISKS.md R02), build upstream, install into a
   dedicated test OBS profile.
4. Use the synthetic media generator and benchmark runner (once Phase 2
   harness exists) to run S1–S5, 5 repetitions each, warm-up 10 s / window
   60 s, recording CPU, RSS, latency (mean/p99), pacing jitter/drops/dupes,
   bandwidth and A/V sync.
5. Write `FINDINGS.md` with raw numbers, methodology deviations, and what was
   learned. Update PERFORMANCE_TARGETS.md baseline column and PROGRESS.md.

**Note on ordering:** the harness is built in Phase 2, after recon. If the
full harness is not ready, `exp_000` records that limitation and uses the
simplest reproducible setup (manual synthetic input + OS performance
counters), with all deviations documented rather than hidden.

## Success criteria for this experiment

- Baseline numbers exist for at least S2 (1080p30) and S3 (1080p60).
- Raw data file(s) are committed or their regeneration script is committed.
- FINDINGS.md states which hypotheses were confirmed and which were wrong.
