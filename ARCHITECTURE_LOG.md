# ARCHITECTURE LOG — locked decisions

Every entry: date, decision, alternatives considered, evidence (link to
`exp_NNN/`), consequences. A locked decision is reversed only by a new entry
citing new evidence. Never silently change course. Newest entries first.

---

## 2026-11-22 — Project name and repository

- **Decision:** project is named **Wormhole**; repository is
  `https://github.com/limapedro/wormhole` (public).
- **Alternatives considered:** other names were not seriously considered; the
  name is fixed by the standing contract.
- **Evidence:** `gh repo view limapedro/wormhole` failed before creation
  (name free), `gh repo create` succeeded; initial commit `eeabbb8`.
- **Consequences:** all docs and CI use this name; no other repository is
  touched.

## 2026-11-22 — Working methodology

- **Decision:** adopt the standing contract's methodology verbatim:
  hypothesis before experiment (`exp_NNN/HYPOTHESIS.md`), findings after
  (`FINDINGS.md`, including negative results), locked decisions here, typed
  TOML config with minimal flags, evidence over assumption, docs in the same
  commit series, small reversible steps, always-green `main`.
- **Alternatives considered:** ad-hoc exploration, relaxing documentation for
  speed. Rejected: the mission explicitly requires reproducible evidence.
- **Evidence:** standing master prompt; `exp_000/HYPOTHESIS.md`.
- **Consequences:** any task lacking an experiment file when measurement or
  design is involved is incomplete; failed experiments are never deleted.

## 2026-11-22 — Initial language / architecture direction (provisional)

> **This is the starting default from the contract, not yet an evidence-backed
> lock.** It is recorded here so the starting point is explicit. Phase 3 must
> confirm or overturn it with a new entry citing an `exp_NNN/`.

- **Decision (provisional):** a thin C layer that talks to libobs (C API),
  plus a **Rust core** for networking, framing, buffering and codecs, linked
  as a static library. No garbage collector and no cgo-style boundary cost on
  the hot path.
- **Alternatives considered:** pure C/C++ core (simpler FFI, weaker safety and
  ergonomics); pure Rust including OBS binding (still needs C ABI calls);
  Go (matches upstream, but GC pauses and cgo boundary cost conflict with the
  latency/allocation targets).
- **Evidence:** contract section 5 starting defaults; dev machine has Rust
  1.99.0 and CMake 4.3.3 (recorded in PERFORMANCE_TARGETS.md). **No benchmark
  yet.**
- **Consequences:** design C ABI boundary early to keep it off the hot path;
  revisit with measurements in Phase 3 before committing large amounts of
  code.

## 2026-11-22 — Hot-path and wire direction (provisional)

- **Decision (provisional):** preallocated buffer pools and ring buffers, no
  per-frame heap allocation in steady state; `TCP_NODELAY` and tuned socket
  buffers with an explicit drop-frames-never-block-render-thread backpressure
  policy; two wire modes — Teleport-compatible and Wormhole protocol v2 with
  a versioned handshake that negotiates down automatically.
- **Alternatives considered:** per-frame allocation with a GC (rejected:
  allocation/latency targets); blocking backpressure (rejected: hard rule
  about OBS threads); v2-only wire format (rejected: parity and differential
  testing require compatibility mode).
- **Evidence:** contract sections 3, 5, 7, 13. **No benchmark yet.**
- **Consequences:** protocol v2 spec must reserve version/capability fields
  from day one so compatibility mode can be negotiated rather than bolted on.

## 2026-11-22 — License decision

- **Status: NOT YET DECIDED.** Blocked on Phase 1 license check. See
  [RISKS.md](RISKS.md). The LICENSE file is a placeholder and no
  upstream-derived code may be published until this entry is finalized.
