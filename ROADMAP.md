# ROADMAP — Wormhole

Phased plan. Each phase maps to a GitHub milestone. Issue links are filled in
once the milestones and issues are created from this document after the docs
PR merges (see "Issue map" per phase; `TBD` entries must be replaced).

Work follows the loop in the standing contract: read
[PROGRESS.md](PROGRESS.md), pick the highest-priority open issue, experiment
first when the task involves measurement or design, work on a short-lived
branch, record findings, update docs, open a PR, merge.

## Phase 0 — Setup (milestone `0 — Setup`)

**Goal:** repository, documentation, issues, CI skeleton. No plugin code.

- [x] Verify `gh` access (done: `limapedro`, scopes `repo`, `workflow`).
- [x] Create public repo `limapedro/wormhole`.
- [x] Initial commit: README, .gitignore, license placeholder.
- [ ] Docs-only PR: all section-3 planning documents.
- [ ] Create milestones and GitHub issues from this roadmap.
- [ ] CI skeleton on a code branch (not this docs PR).

**Acceptance criteria:** all planning docs have real content; every roadmap
item has an issue; no code merged before the docs PR.

**Dependencies:** none.

**Issue map:** setup tasks tracked as milestone `0 — Setup`, issues TBD.

## Phase 1 — Reconnaissance (milestone `1 — Recon`)

**Goal:** understand original Teleport completely, with local evidence.

- Clone upstream Go `obs-teleport` into a git-ignored `third_party/` directory;
  record exact commit hash in [PROTOCOL.md](PROTOCOL.md).
- License check: read upstream LICENSE; decide derivative vs clean-room;
  choose Wormhole license; log in
  [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md); flag uncertainties in
  [RISKS.md](RISKS.md).
- Read entire upstream codebase; produce feature inventory; fill the parity
  checklist in [GOAL.md](GOAL.md).
- Build upstream on Windows; document exact steps and problems in
  [HARNESS.md](HARNESS.md); install into a dedicated test OBS profile.
- Write `exp_000/HYPOTHESIS.md` (done) and run baseline benchmarks; record
  `exp_000/FINDINGS.md`.

**Acceptance criteria:** parity checklist has no `unverified` rows; upstream
commit hash recorded; license decision logged; baseline numbers captured;
unverified architecture hypotheses listed in `PROTOCOL.md`.

**Dependencies:** Phase 0. **Blockers to watch:** Go toolchain not installed
on the dev machine (see [RISKS.md](RISKS.md)).

**Issue map:** issues TBD (labels `research`, `docs`, `harness`,
`platform:windows`).

## Phase 2 — Harness (milestone `2 — Harness`)

**Goal:** make Teleport fully observable and reproducible without a human.

Components (details in [HARNESS.md](HARNESS.md)):

- [ ] Transparent TCP (+ UDP/mDNS) capture proxy with delay/drop/bandwidth
      injection and pcap export.
- [ ] Protocol dissector with decode→encode byte-identical round-trip test.
- [ ] Instrumented upstream build (separate branch/patch set, never mixed into
      Wormhole code): frame timings, allocations, GC pauses, cgo calls, queue
      depths.
- [ ] Mock OBS host for headless CI testing (or documented headless-OBS
      fallback and its limits).
- [ ] Synthetic media generator: color bars / counter / motion at
      720p/1080p/1440p/4K, 30/60 fps; tones with sync markers, multi-channel,
      multiple tracks.
- [ ] Reference sender and receiver for the original protocol.
- [ ] Replay tool.
- [ ] Differential tester (output, timing, bandwidth, failure behavior).
- [ ] Benchmark runner writing CSV/JSON + human summary into `exp_NNN/`.

**Acceptance criteria:** every question in section 5 of the contract is
answered with evidence in [PROTOCOL.md](PROTOCOL.md); dissector round-trips
byte-identically; harness runs unattended in CI.

**Dependencies:** Phase 1. **Issue map:** issues TBD (label `harness`).

## Phase 3 — Architecture (milestone `3 — Architecture`)

**Goal:** lock architecture decisions with evidence; draft protocol v2.

- Confirm or overturn each default in section 5 of the contract: thin C layer
  + Rust core static lib; zero-copy/zero-alloc hot path; SIMD JPEG
  (libjpeg-turbo) evaluation; dedicated threads vs async runtime; socket
  tuning; Teleport-compatible mode + Wormhole protocol v2 with versioned,
  auto-negotiated handshake; never block OBS render/audio threads.
- Each decision gets an `ARCHITECTURE_LOG.md` entry citing an `exp_NNN/`.
- Draft Wormhole protocol v2 spec in [PROTOCOL.md](PROTOCOL.md).

**Acceptance criteria:** no architecture decision lacks evidence; v2 spec
covers capability negotiation and multi-audio descriptors.

**Dependencies:** Phase 2. **Issue map:** issues TBD (labels `protocol`,
`perf`, `multi-audio`).

## Phase 4 — Parity, Windows first (milestone `4 — Parity`)

**Goal:** full Teleport feature parity in compatibility mode on Windows.

- [ ] Sender + receiver in compatibility mode.
- [ ] Discovery.
- [ ] Source / filter / output equivalents.
- [ ] Settings UI.
- [ ] Full parity checklist green.
- [ ] Differential tests passing against original Teleport.

**Acceptance criteria:** all P01–P24 rows `verified`; differential tester
green; CI green on Windows.

**Dependencies:** Phases 1–3. **Issue map:** issues TBD (labels `protocol`,
`video`, `audio`, `platform:windows`).

## Phase 5 — Efficiency (milestone `5 — Efficiency`)

**Goal:** profile-driven optimization until every target in
[PERFORMANCE_TARGETS.md](PERFORMANCE_TARGETS.md) is met.

- Every optimization is its own experiment with `exp_NNN/HYPOTHESIS.md` and
  `FINDINGS.md`.
- Focus areas: buffer pools / ring buffers, allocation elimination, SIMD
  codec, socket tuning, backpressure policy, framing overhead.

**Acceptance criteria:** targets met, each claim linked to a committed
benchmark result; no OBS render-thread stalls attributable to the plugin.

**Dependencies:** Phase 4. **Issue map:** issues TBD (label `perf`).

## Phase 6 — Multi-audio (milestone `6 — Multi-audio`)

**Goal:** the headline feature, per section 9 of the contract.

- [ ] Decide mapping to OBS (six mixer tracks vs sources vs both) with
      evidence; log in `ARCHITECTURE_LOG.md`.
- [ ] Sender: per-track selection/naming/sample rate/layout/toggle, shared
      clock timestamps.
- [ ] Receiver: chosen presentation (companion sources / mixer routing),
      evaluated with evidence.
- [ ] Protocol v2: stream descriptors, track IDs, negotiation, fallback to
      ordinary Teleport audio for track 1.
- [ ] Sync/robustness: sample-accurate tracks, mid-session add/remove, mixed
      sample rates.
- [ ] Tests: synthetic multi-tone multi-track audio, fidelity and skew
      assertions.

**Acceptance criteria:** cross-track skew under target; compatibility peer
still receives track 1; all tests green.

**Dependencies:** Phase 4 (and ideally Phase 5). **Issue map:** issues TBD
(label `multi-audio`, `audio`, `protocol`).

## Phase 7 — Linux and macOS (milestone `7 — Platforms`)

- [ ] Ports and platform-specific audio/video paths.
- [ ] CI green on `windows-latest`, `ubuntu-latest`, `macos-latest`, and Apple
      Silicon runner if available.
- [ ] Installers: Windows installer, Linux package/tarball, macOS signed and
      notarized bundle; notarization steps in [PLATFORMS.md](PLATFORMS.md).

**Acceptance criteria:** all platform notes accurate; anything unverifiable
on real hardware flagged in [RISKS.md](RISKS.md).

**Dependencies:** Phase 4; parallel with 5/6 where possible. **Issue map:**
issues TBD (labels `platform:linux`, `platform:macos`).

## Phase 8 — Hardening (milestone `8 — Hardening`)

- Fuzz every network-input parser; 24h+ soak; chaos (loss, reorder, delay,
  bandwidth caps, abrupt disconnects); leak checks; crash recovery.

**Acceptance criteria:** no crash/freeze under any harness-generated failure;
all sanitizer/soak tests pass. **Dependencies:** Phases 4–7.

**Issue map:** issues TBD (labels `harness`, `perf`).

## Phase 9 — Release (milestone `9 — Release`)

- Semantic-version releases, CI-built artifacts for all three platforms with
  checksums; release notes from [PROGRESS.md](PROGRESS.md); user docs,
  migration guide, upstream attribution.

**Acceptance criteria:** `v0.1.0` and later tagged; artifacts attached;
docs complete. **Dependencies:** Phase 8.

**Issue map:** issues TBD (label `docs`).
