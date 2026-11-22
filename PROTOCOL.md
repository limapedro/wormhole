# PROTOCOL — Teleport wire protocol (reverse engineering) & Wormhole v2

## Purpose

This document is the byte-accurate specification of **original Teleport's**
wire protocol, built from evidence captured by the harness. When complete, a
third party must be able to write a compatible receiver from it alone.

The **Wormhole protocol v2** specification lives in a separate section below.
It is a draft until Phase 3 locks it.

> **Status: SKELETON.** No upstream facts are asserted yet. Every `Unknown`
> field is filled in during Phases 1–2 with a link to an `exp_NNN/` artifact.
> Descriptions in the master prompt are hypotheses, not facts.

## Reference artifact

- Upstream repository: Go-based `obs-teleport` (to be cloned into git-ignored
  `third_party/`).
- Exact commit hash: **Unknown — record during Phase 1.**
- Upstream version/tag: **Unknown.**
- Instrumented build patch set location: **Unknown (Phase 2).**

## Part 1 — Original Teleport protocol

### 1.1 Discovery

| Question | Answer |
|---|---|
| Discovery mechanism | Unknown (hypothesis: mDNS/zeroconf) |
| Service type | Unknown |
| TXT records | Unknown |
| Ports advertised | Unknown |
| Discovery interval / timeouts | Unknown |
| Manual connect fallback | Unknown |

### 1.2 Connection and handshake

Unknown. Reserve space here for:

- connection direction and transport (TCP/UDP per stream),
- ordered message sequence diagram (mermaid),
- version/capability exchange, if any,
- error paths and timeouts.

### 1.3 Header layout and message types

Unknown. Reserve a byte-accurate table:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| — | — | — | Unknown |

Message type enum: Unknown.

### 1.4 Video

| Question | Answer |
|---|---|
| Codec | Unknown (hypothesis: JPEG-family) |
| Quality settings and units | Unknown |
| Pixel formats | Unknown |
| Resolution / FPS negotiation | Unknown |
| Frame framing, markers, sequence numbers | Unknown |
| Timestamps and clock source | Unknown |
| Frame drop / backpressure | Unknown |

### 1.5 Audio

| Question | Answer |
|---|---|
| Sample format / rate / channels | Unknown |
| Codec | Unknown (hypothesis: Opus) |
| Chunking | Unknown |
| Timestamps and clock source | Unknown |
| Number of tracks/streams supported | Unknown |

### 1.6 A/V synchronization

Unknown. Which clock, which timestamp domain, how drift is handled.

### 1.7 Disconnect, reconnect, slow receivers

Unknown. Sender behavior when network cannot keep up: Unknown.

### 1.8 State machines

Reserve mermaid diagrams for sender and receiver states, including connect,
streaming, degraded, reconnect and error. Fill in Phase 2.

### 1.9 Profiling summary (where CPU/memory go)

Unknown. From instrumented build in Phase 2.

### 1.10 Evidence index

| Experiment | Question answered | Artifact |
|---|---|---|
| `exp_000` | baseline metrics | pending |
| `exp_001` | TBD | pending |

## Part 2 — Wormhole protocol v2 (draft)

> Status: draft. Locks in Phase 3 via `ARCHITECTURE_LOG.md`.

Design requirements already fixed by the contract:

- Versioned handshake that negotiates down to Teleport-compatible mode
  automatically.
- Stream descriptors carrying track IDs, names, formats.
- Capability negotiation.
- Multi-audio: several independent streams, sample-accurate relative to each
  other and to video; mixed sample rates supported.
- Compatibility fallback: a v2 sender talking to a legacy peer delivers
  track 1 as ordinary Teleport audio.

Wire layout, message enums, state machines and error semantics: **to be
drafted in Phase 3** once Part 1 is complete. No fields are specified here yet
so the design is driven by measured Teleport behavior rather than assumption.
