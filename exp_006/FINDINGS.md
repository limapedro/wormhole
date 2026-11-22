# exp_006 — Findings: two portable OBS instances, live Teleport capture, CPU and image quality

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Scripts/configs:** `scripts/setup_portable_obs.ps1`, `scripts/run_two_obs.ps1`, `harness/config/obs-proxy.toml`, `harness/config/obs-quality.toml`

## What happened

- Two self-contained portable OBS copies were created under git-ignored
  `third_party/obs-portable/{sender,receiver}` (606.2 MB each), each with
  `portable_mode.txt`, the Teleport 0.8.0 DLL, a 1280x720/30 profile, and
  generated scene collections. The receiver uses manual IP `127.0.0.1:18080`
  (the proxy); the sender listens on TCP 18081.
- Both instances launched simultaneously with `--portable --multi --profile
  Wormhole --collection ...`. Logs confirm in **both**:
  `[obs-teleport] version: 0.8.0, go: go1.27.1, jpeg: 3.2.0` and
  `obs-teleport.dll` in the loaded-plugin list. Sender logs `connect:
  127.0.0.1:…` / `output started`; receiver logs `connected to:
  127.0.0.1:18080`.
- The proxy captured the sender→receiver direction:
  **8,389,332 bytes**, which the dissector decoded into **768 packets
  (300 JPEG + 468 WAVE)** with **byte-identical round-trip**. First packets:
  JPEG timestamp 213648931502890 ns, 15,031-byte payload, partial range;
  WAVE same timestamp, format 4 (FLOAT), 48,000 Hz, 2 speakers, 1,024
  frames (8,192 bytes). Timestamp span ≈ 9.967 s.
- CPU sampled over a 6.02 s window (12 logical CPUs): receiver **0.453 s**
  CPU time (0.63 % of machine), sender **1.125 s** (1.56 %). Working sets:
  266.1 MB receiver, 275.5 MB sender. Raw rows appended to
  `exp_results/two-obs-cpu.csv` (git-ignored).
- Image quality: the `quality` command decoded all 300 JPEG frames at
  1280x720. Mean RGB `(18.0, 1.0, 230.0)`; MSE vs each frame's own mean
  `0.000`, PSNR capped at 120 dB — the colour source produces a uniform
  frame and JPEG preserved it exactly. Report: `exp_results/obs-quality.json`.

## The plugin-error dialog (user screenshot) — diagnosed

The dialog is **not Teleport**. Both logs list these unrelated failures:
`aja.dll`, `aja-output-ui.dll`, `decklink.dll`, `decklink-captions.dll`,
`decklink-output-ui.dll` (capture hardware SDKs absent) and
`multi-source-record.dll` (localisation load failure). Teleport loads and
streams in the same log, and the 8.3 MB capture proves it. No fix to
obs-teleport for OBS 33 is warranted by this evidence.

## Negative results (recorded, not hidden)

- First attempt captured **0 bytes**: the proxy only recorded client→target,
  but with a Teleport receiver as the TCP client the media flows
  target→client. Fixed by adding `capture_down` to `ProxyConfig` and having
  the proxy record both directions.
- Removing the failing `aja`/`decklink`/`multi-source-record` modules from the
  portable copies produced a worse failure: `Failed to load core module aja`
  and OBS did not reach scene load, so no stream was captured. Restored the
  original modules; streaming worked again. The harmless dialog is preferred
  over a blocked startup.
- The first `quality` run emitted `psnr_db: null` because JSON cannot encode
  infinity (MSE was exactly zero). Fixed by capping PSNR at 120 dB while
  keeping raw MSE.

## What was learned

All three hypotheses were confirmed. The two-instance setup is real,
unattended (script-driven) and produces a decodable live Teleport stream with
CPU and image-quality measurements. This is the first evidence in the project
from a real Teleport 0.8.0 binary rather than source reading or synthetic
fixtures. It confirms the source-derived wire facts on the live stream:
JPEG/WAVE framing, 48 kHz float 2-channel audio with 1,024-frame chunks, and
nanosecond timestamps.

Limitations: these are **Teleport** numbers on this machine, not Wormhole
numbers, and no `exp_000` comparison exists yet. The colour source is
uniform, so the quality metric measures uniformity/fidelity, not complex-image
PSNR/SSIM. The plugin-error dialog remains visible but is unrelated.

## Consequences

- The two-portable-OBS monitor is now a repeatable harness workflow
  (`scripts/setup_portable_obs.ps1` + `scripts/run_two_obs.ps1` + `dissect` +
  `quality`).
- Next: use this setup to capture the baseline (`exp_000`) and later compare
  Wormhole against Teleport under identical conditions.
