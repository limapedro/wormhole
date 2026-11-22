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

> **Status of this checklist:** this is the *starting hypothesis* derived from
> the project description, written before reconnaissance. Every row must be
> confirmed or corrected against the cloned upstream source in Phase 1 and the
> status changed from `unverified`. A third party must be able to implement a
> compatible receiver from [PROTOCOL.md](PROTOCOL.md) plus this checklist.

Legend: `unverified` | `confirmed` | `verified` (confirmed + differential
test evidence) | `N/A` (does not exist in upstream).

| # | Feature area | Expected Teleport feature (hypothesis) | Status | Evidence |
|---|---|---|---|---|
| P01 | Sender | A source/output that sends program output to peers | unverified | — |
| P02 | Receiver | A source that receives and renders video in OBS | unverified | — |
| P03 | Filter | Any OBS filter exposed by the plugin | unverified | — |
| P04 | Output | Any OBS output exposed by the plugin | unverified | — |
| P05 | Discovery | Automatic peer discovery (mDNS/zeroconf) | unverified | — |
| P06 | Discovery | Service type, TXT records and port layout | unverified | — |
| P07 | Connection | Manual connection by host/IP and port | unverified | — |
| P08 | Connection | Multiple simultaneous receivers from one sender | unverified | — |
| P09 | Video | Codec used (JPEG-family hypothesis) | unverified | — |
| P10 | Video | Quality/bitrate control and its units | unverified | — |
| P11 | Video | Resolution / FPS negotiation or pass-through | unverified | — |
| P12 | Video | Frame drop / backpressure policy | unverified | — |
| P13 | Audio | Codec, sample format, rate, channels | unverified | — |
| P14 | Audio | Audio chunking and timestamps | unverified | — |
| P15 | Sync | A/V synchronization mechanism and clocks | unverified | — |
| P16 | Reliability | Disconnect / reconnect behavior | unverified | — |
| P17 | Reliability | Slow-receiver behavior | unverified | — |
| P18 | Settings | Every exposed setting and its default | unverified | — |
| P19 | UI | Every UI element and its behavior | unverified | — |
| P20 | Transport | TCP vs UDP per stream; ports used | unverified | — |
| P21 | Handshake | Connection and capability handshake sequence | unverified | — |
| P22 | Platform | Windows build/install steps | unverified | — |
| P23 | Platform | Linux and macOS support in upstream | unverified | — |
| P24 | Error handling | Behavior on bad network / malformed input | unverified | — |

Each row becomes or maps to a GitHub issue when issues are created after the
docs PR merges. The checklist is kept in sync with `PROTOCOL.md` and the
issue tracker; no row is marked `verified` without a linked artifact.
