# GOAL — Wormhole

## Mission

Send OBS video and audio from one machine (or one OBS instance) to another over
a network with the lowest practical latency, CPU use, memory use and overhead.
Match Teleport feature-for-feature first, then beat it on every measurable
axis, then add multiple simultaneous audio outputs.

## Success definition

Wormhole is successful when all three are true:

1. **Parity:** every Teleport feature in the parity checklist below is marked
   `verified` with linked evidence (differential test or harness capture).
2. **Efficiency:** every performance target in
   [PERFORMANCE_TARGETS.md](PERFORMANCE_TARGETS.md) is met or the target is
   revised with evidence and logged in
   [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md).
3. **Multi-audio:** a sender can transmit several independent, sample-accurate
   audio streams and a receiver can use them separately, with cross-track skew
   under the target, on all three platforms.

Parity and correctness gate efficiency: a fast plugin that is not compatible
is not a success.

## Priorities (strict order)

1. **Feature parity with Teleport.**
2. **Large, measured efficiency gains.**
3. **Multiple simultaneous audio outputs.**

Work on a lower-priority item does not start while a higher-priority item is
blocked unless the blocker is recorded in [RISKS.md](RISKS.md) and the issue is
labelled `blocked` (per the working loop in the standing contract).

## Explicit non-goals

- Being a general-purpose streaming platform (RTMP, SRT, HLS, WebRTC
  ingest). Wormhole is a point-to-point OBS-to-OBS (and OBS-instance-to-OBS-
  instance) tool.
- Replacing OBS's own recording or local output paths.
- Cloud relay / NAT traversal services. Direct network paths only for v1;
  a relay is out of scope unless a separate decision is logged.
- GPU-assisted encoding as a requirement. It may be an optional feature
  later; correctness never depends on it.
- Windows-only design. Platform-specific code stays behind small interfaces.
- Reimplementing the OBS UI. Wormhole ships OBS plugin UI only.

## Parity checklist — original Teleport

> **Status of this checklist:** all rows were confirmed against the pinned
> upstream source during Phase 1 (2026-11-22). See the evidence column and
> [PROTOCOL.md](PROTOCOL.md), which is now source-complete enough to implement
> a compatible receiver. `confirmed` means source-derived; `verified` is
> reserved for rows with differential-test evidence, which Phase 2/4 will add.

Legend: `unverified` | `confirmed` (source evidence) | `verified` (confirmed +
differential test evidence) | `N/A` (does not exist in upstream).

| # | Feature area | Teleport feature (confirmed) | Status | Evidence |
|---|---|---|---|---|
| P01 | Sender | `teleport-output` OBS output; also A/V filter sender | confirmed | `main.go:185-196`, `output.go:54-102` |
| P02 | Receiver | `teleport-source` async A/V source with `obs_source_output_video2`/`audio` | confirmed | `main.go:132-143`, `source.go:317,355` |
| P03 | Filter | Three filters: `teleport-filter` (A/V), `teleport-video-filter`, `teleport-audio-filter` | confirmed | `main.go:145-183`, `filter.go:55-68` |
| P04 | Output | `teleport-output`, flags `OBS_OUTPUT_AV`, raw video+audio, dropped-frame getter | confirmed | `main.go:185-196` |
| P05 | Discovery | `schollz/peerdiscovery` UDP multicast — **not mDNS** (hypothesis overturned) | confirmed | `announcer.go:60-64`, `discoverer.go:43-47` |
| P06 | Discovery | Multicast `239.255.255.250:9999`, 1 s interval, raw JSON payload with Name/Port/AudioAndVideo/Version | confirmed | `peerdiscovery/internal.go:22-40`, `types.go:31-37` |
| P07 | Connection | Manual `manual-ip` (`host:port`) plus `has-audio-and-video` toggle; list takes discovery data | confirmed | `source.go:472-475`, `source.go:79-84` |
| P08 | Connection | Multiple simultaneous receivers; per-connection goroutine and 1000-buffer channel | confirmed | `sender.go:39-68` |
| P09 | Video | JPEG via libjpeg-turbo 3.2.0 `tj3`, SIMD build | confirmed | `packet.go:53-124`, `build.yaml:29-36` |
| P10 | Video | Integer quality 1–100, default 90, warning above 90; no bitrate setting | confirmed | `frontend.go:192-199,214` |
| P11 | Video | No resolution/FPS negotiation; dimensions from JPEG SOF, timestamps from OBS | confirmed | `packet.go:130-134`, `types.go:39-43` |
| P12 | Video | Encode queue >1 s increments dropped frames; per-connection drop above 800 queued buffers | confirmed | `output.go:165-170`, `sender.go:83-88` |
| P13 | Audio | Raw interleaved PCM, no codec (Opus hypothesis overturned); OBS format/speaker enums on wire | confirmed | `packet.go:450-549` |
| P14 | Audio | Chunk = one OBS audio callback; `Frames`, sample rate and format in 16-byte `WaveHeader`; OBS ns timestamp | confirmed | `packet.go:479-495`, `types.go:51-56` |
| P15 | Sync | Sorted timestamp queue; A/V pairing gate; one-time offset; no drift correction | confirmed | `source.go:235-396` |
| P16 | Reliability | 100 ms dial timeout, 100 ms retry, state reset on reconnect, EOF breaks read loop | confirmed | `source.go:490-511,530-564` |
| P17 | Reliability | Slow receiver: queue warnings at 100, drops above 800; write error disconnects | confirmed | `sender.go:57-91` |
| P18 | Settings | Sender: enabled, identifier, quality, show advanced, port (defaults false/""/90/false/0). Receiver: stream list, show advanced, manual IP, has-audio-and-video (defaults ""/false/""/false) | confirmed | `frontend.go:180-217`, `source.go:185-215` |
| P19 | UI | Tools→Teleport menu, dummy property source, refresh list button, quality/enabled warnings, filter Apply button | confirmed | `frontend.go:67-113,180-207`, `filter.go:103-126` |
| P20 | Transport | TCP for media on configurable port (0=auto); UDP multicast for discovery | confirmed | `output.go:235`, `sender.go`, `peerdiscovery` |
| P21 | Handshake | **None** — no greeting, version or capability exchange on the wire | confirmed | `source.go:530-562`, `output.go:247-258` |
| P22 | Platform | Windows build: cross-compiled via mingw in Docker CI; local build blocked (no C compiler) | confirmed | `build.yaml:17,46-67`, `HARNESS.md` |
| P23 | Platform | Linux and macOS builds in upstream CI (x86_64/arm64, macOS 13 deployment target) | confirmed | `build.yaml:11-18` |
| P24 | Error handling | Corrupt JPEG panic recovered and packet discarded; stream panic restarts read loop after 1 s | confirmed | `source.go:256-270,434-443` |

Each row becomes or maps to a GitHub issue when issues are created after the
docs PR merges. The checklist is kept in sync with `PROTOCOL.md` and the
issue tracker; no row is marked `verified` without a linked artifact.
