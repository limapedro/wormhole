# exp_011 — Findings: 1440p gaming PC → 1080p streaming PC over a simulated 40 Mbit/s link

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Orchestration:** OBS-MCP tools through `config/gaming-streaming.toml`; capture config `harness/config/obs-proxy-40m.toml`.

## What happened

- Both portable OBS instances launched via OBS-MCP `obs_launch` with no
  Plugin Errors dialog. `GetVideoSettings` over OBS WebSocket v5 reported:
  - sender: base **2560x1440**, output **1920x1080**, 60 fps
    (`fpsNumerator=60`, `fpsDenominator=1`);
  - receiver: base and output **1920x1080**, 60 fps.
- Sources were created/confirmed on the sender through `CreateInput` /
  `GetInputList`: **Display** (`monitor_capture`, OBS 33's Display Capture),
  **DesktopAudio** (`wasapi_output_capture`), **MicOne** and **MicTwo**
  (`wasapi_input_capture`). `GetSourceActive` returned
  `videoActive=true, videoShowing=true` for all four. WASAPI logs confirm all
  three audio devices initialised at 48 kHz (mics) and 96 kHz (desktop).
- The receiver connected to the proxy (`[obs-teleport] connected to:
  127.0.0.1:18080`) and the sender started output (`connect: 127.0.0.1:…`,
  `output started`).
- **Clean 12-second stream** captured through the 40 Mbit/s capped proxy:
  `obs-workflow-capture.bin` 34,282,274 bytes. C dissector: **1,427 packets
  (802 JPEG + 625 WAVE)**, byte-identical round-trip, duration **13.35 s**,
  average **2,567,961 bytes/s = 20.54 Mbit/s** — below the 40 Mbit/s cap.
  Average JPEG payload 36,233 bytes.
- Image quality: all **802 frames decoded at 1920x1080**; mean RGB
  (16.5, 12.2, 69.5). `PSNR=13.55 dB` is measured against each frame's own
  mean, so for live desktop content it is a uniformity statistic, **not** a
  fidelity score against a known source (see limitation below).
- CPU over a 2 s window (12 logical CPUs): sender 0.922 s (3.84 %, 371.8 MB),
  receiver 0.766 s (3.19 %, 312.0 MB). OBS `GetStats` over the same period:
  sender 60.0 fps, CPU 4.27 %, 354.8 MB, 484 rendered / 0 skipped frames,
  432 output frames; receiver 60.0 fps, CPU 2.84 %, 304.2 MB, 484 rendered /
  0 skipped frames.

## What was learned

All hypotheses were confirmed: the workflow is reproducible, sources are
active, the stream is decodable at 1080p, bandwidth stays under the configured
cap, and both OBS processes can be sampled through MCP. The first source-kind
attempt failed (`display_capture` is not valid in OBS 33; the correct kind is
`monitor_capture`), and the initial WebSocket client failed on `GetStats`
because it never consumed OBS's `Identified` reply — both were fixed and
re-verified.

Limitations: the 40 Mbit/s cap was not saturated (20.54 Mbit/s average), so
this does not characterise behaviour at the limit. `GetStreamStatus` reports
0 output frames because Teleport is an OBS *output* driven directly by the
plugin, not an OBS stream output; wire frame counts are the reliable measure.
Quality is not PSNR against the source desktop image.

## Consequences

- A repeatable OBS-MCP workflow config and script now exist for this scenario.
- Future work: saturate the 40 Mbit/s cap with a higher-quality/fps setting and
  measure pacing under congestion; add a synthetic frame counter for true
  end-to-end latency.
