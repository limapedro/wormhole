# exp_013 — Findings: Wormhole plugin load path and v2 protocol foundation

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)

## Plugin build and load

- OBS 33.0.0 source archive (15.5 MB) downloaded into git-ignored
  `third_party/obs-sdk`; `libobs` headers extracted and an empty `obsconfig.h`
  created (the same trick upstream CI uses).
- `obs.dll` exports parsed with MSVC `dumpbin` (1,781 usable names after
  fixing the column regex) and turned into `third_party/obs-sdk/lib/obs.lib`
  with `lib.exe`; the reproducible script is `scripts/make_obs_import_lib.ps1`.
- `plugin/src/plugin.c` compiled with MSVC `/W4 /WX /wd4201` (the only
  suppressed warning is a nameless-union warning inside libobs's own headers)
  into `build/plugin/wormhole.dll` (104,960 bytes).
- After copying the DLL into a portable OBS plugin directory and launching the
  instance through OBS-MCP, the OBS log contained:

```
[obs-teleport] version: 0.8.0, go: go1.27.1, jpeg: 3.2.0
[wormhole] version 0.1.0
    wormhole.dll
    obs-teleport.dll
```

The plugin loads and registers `wormhole-source` (async A/V input) and
`wormhole-output` (AV output). No dialog or crash.

## v2 protocol foundation

- `include/wormhole/protocol_v2.h` + `core/c/protocol_v2.c` implement a 28-byte
  framed protocol with magic `WH02`, version, total length, sequence and
  nanosecond timestamp. Messages: HELLO, HELLO_ACK, STREAM_DESC, VIDEO, AUDIO,
  PING, GOODBYE. Design is documented in `docs/WORMHOLE_V2.md`.
- Unit tests (`test_protocol_v2`) pass round-trips for all message types and
  reject bad magic/version/length/truncation.
- TCP negotiation test (`test_v2_negotiation`) passed:

```
negotiation: HELLO from 'gaming-pc' with 3 tracks
negotiation: ACK accepted v2, capabilities=0x5
stream 10: 'DesktopAudio' 48000 Hz 2 ch 16-bit clock=1
stream 11: 'MicOne' 48000 Hz 2 ch 16-bit clock=1
stream 12: 'MicTwo' 48000 Hz 2 ch 16-bit clock=1
video: 1920x1080, 4 bytes
V2 NEGOTIATION TEST PASSED: 3 descriptors, 3 audio frames
```

This is the concrete "step beyond Teleport": Teleport has one undifferentiated
PCM stream and no negotiation; Wormhole v2 carries named, clocked, independently
identified tracks and versioned capabilities, with automatic fallback to
Teleport-compatible `JPEG`/`WAVE` framing.

## Negative results

- First `.def` generation captured RVA columns instead of export names, giving
  1,691 bogus symbols and five unresolved externals; fixed the regex (1,781
  real exports) and the DLL linked.
- MSVC `/W4 /WX` rejected libobs's nameless unions; suppressed only `C4201` for
  vendor headers.
- The first TCP test reader validated `total_len` before receiving the payload;
  fixed to read the declared length first.

## What is still missing

The plugin skeleton registers empty source/output callbacks — it does **not**
yet transmit or receive real media. The v2 protocol library and negotiation are
proven, but JPEG conversion, socket integration in the OBS callbacks,
multi-audio routing, settings UI and full Teleport compatibility mode in the
plugin remain to be implemented and verified.

## Consequences

This is the foundation, not completion. Next work: wire `core/c/protocol_v2.c`
into `wormhole-output`/`wormhole-source`, add YUV→JPEG conversion and audio
track settings, then demonstrate a real Wormhole-to-Wormhole stream in the two
portable OBS instances.
