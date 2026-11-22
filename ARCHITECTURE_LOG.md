# ARCHITECTURE LOG — locked decisions

Every entry: date, decision, alternatives considered, evidence (link to
`exp_NNN/`), consequences. A locked decision is reversed only by a new entry
citing new evidence. Never silently change course. Newest entries first.

---

## 2026-11-22 — All-C migration complete: Rust workspace removed

- **Decision:** the Rust workspace is deleted from `main`. The repository ships
  a single language, C, for the protocol core and the entire harness.
- **Alternatives considered:** keeping Rust as a deprecated secondary harness;
  rejected because parity was demonstrated for every command and two
  toolchains would violate the user's directive.
- **Evidence:** `exp_005` (MSVC C core builds, C↔Rust wire equivalence),
  `exp_007` (C dissector matches Rust on live capture), `exp_008` (C proxy),
  `exp_009` (C quality/differential), `exp_010` (C media + reference peers,
  all C tests pass). CI builds C on Windows, Linux and macOS with warnings as
  errors.
- **Consequences:** historical experiments remain as evidence; new work uses
  only `scripts/build_c.bat` / `scripts/build_c.sh` and the C harness.

## 2026-11-22 — All-C implementation (user directive; overturns Rust default)

- **Decision:** Wormhole is implemented in **C**, including the plugin core,
  the protocol/framing library and the harness. The Rust starting default in
  the master prompt is **overturned by explicit user direction**: "I wanted
  all in C." Visual Studio's MSVC compiler is available and will be used to
  compile and test the C code on Windows. Existing Rust harness code is kept
  temporarily only as a reference while its functionality is ported to C; it
  is removed from `main` once the C harness covers the same commands.
- **Alternatives considered:**
  - Thin C libobs layer + Rust core (the prompt's starting default) —
    rejected by the user; it adds a second language, toolchain and ABI
    boundary that the user does not want.
  - Pure C — chosen: libobs is a C API, the wire format is fixed-layout
    little-endian C structs, and MSVC/VS provides a first-class compiler and
    debugger. Nothing in the recon requires Rust.
  - C++ — rejected; user asked for C specifically.
- **Evidence:** user directive in this session (authoritative for this
  decision); `vswhere` reports Visual Studio Community 2026 at
  `C:\Program Files\Microsoft Visual Studio\18\Community`; `cl.exe` 14.42
  found at `VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe`;
  `VC\Auxiliary\Build\vcvars64.bat` present. VERIFIED by compiling C code with
  MSVC in this branch (see `scripts/build_c.bat` output and
  `exp_005/FINDINGS.md`).
- **Consequences:** new `core/c/` C library and `tests/` C tests; MSVC build
  script; CI must build C on all three platforms (MSVC on Windows, clang/gcc
  on Linux/macOS). Performance claims still require benchmarks, and the
  never-block-OBS-threads rule remains unchanged. The reverse-engineered
  protocol and GPL-2.0-or-later license are unaffected.

## 2026-11-22 — License decision: Wormhole is GPL-2.0-or-later

- **Decision:** Wormhole is licensed **GNU GPL version 2, or (at our option)
  any later version** — the same terms as upstream obs-teleport and compatible
  with libobs, which is GPL-2.0-or-later. The placeholder LICENSE is replaced
  with the full GPL-2.0 text. Upstream attribution (Florian Zwoch,
  2021-2026) is recorded in README.md and source headers.
- **Alternatives considered:**
  - **Clean-room reimplementation under a permissive license** — rejected:
    Wormhole links libobs (GPL-2.0-or-later), so the combined work must be
    GPL-compatible; a permissive-only license would not satisfy libobs's
    copyleft obligations.
  - **GPL-3.0-only** — rejected: it would be incompatible with GPL-2.0-only
    parts of the ecosystem and is not needed; "GPL-2.0-or-later" matches both
    upstream and libobs.
  - **Dual license** — rejected as unnecessary complexity for v1.
- **Evidence:** upstream `LICENSE` is GPL-2.0 verbatim; every upstream source
  header states "...either version 2 of the License, or (at your option) any
  later version" (`third_party/obs-teleport/LICENSE:1-7`, `main.go:1-19`).
  libobs headers fetched from OBS 33.0.0 carry the same GPL-2.0-or-later
  notice. No upstream source code was copied into Wormhole; only protocol
  facts were recorded in PROTOCOL.md.
- **Consequences:** all Wormhole source files carry the GPL-2.0-or-later
  notice; releases include the license and upstream attribution; RISKS.md R01
  is resolved. This decision satisfies the contract's requirement that the
  license check be logged before publishing derived code.

## 2026-11-22 — Phase 1 recon: protocol facts and overturned hypotheses

- **Decision:** Teleport compatibility mode must implement the source-derived
  behavior in [PROTOCOL.md](PROTOCOL.md), not the prompt's starting
  hypotheses. Specifically: discovery is UDP multicast on
  `239.255.255.250:9999` with raw JSON (not mDNS); audio is **uncompressed
  interleaved PCM** (not Opus); there is **no wire handshake** or version
  negotiation; video is JPEG via SIMD libjpeg-turbo with quality 1–100;
  timestamps are nanosecond OBS timestamps with a one-time receiver offset.
- **Alternatives considered:** proceeding on the starting hypotheses.
  Rejected — the contract requires locally captured evidence; source reading
  at the pinned commit is that evidence.
- **Evidence:** `third_party/obs-teleport/` HEAD `e8c49f4…`, release tag
  `0.8.0` = `b61ee3c…` (identical source), pinned peerdiscovery `e0ee252…`;
  full findings and line-level citations in PROTOCOL.md Part 1.
- **Consequences:** Wormhole protocol v2 must add a magic/version field,
  total length, sequence numbers and explicit clock domain from the start,
  because Part 1 has none of them. Capture-based confirmation is still
  pending (`C` items in PROTOCOL.md) and is a Phase 2 deliverable. Source
  reading alone does **not** validate performance hypotheses.

## 2026-11-22 — Provisional architecture decisions remain unconfirmed

- **Decision:** the two provisional entries below (language/architecture and
  hot-path/wire direction) are explicitly **not yet locked by evidence**.
  Reconstructed upstream behavior shows the problems to solve (goroutine per
  frame, fresh allocations, blocking send on full queue) but no benchmark has
  compared Rust vs C++ or measured our allocation rate yet.
- **Evidence:** PROTOCOL.md §1.7 and §1.9 (source-derived); no `exp_NNN`
  benchmark exists.
- **Consequences:** Phase 3 must produce experiments before these are marked
  locked; issue #25 et al. remain open.

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

## 2026-11-22 — License decision (superseded)

- **Status: RESOLVED** by the newer entry at the top of this log:
  GPL-2.0-or-later. This placeholder entry is retained for history.
