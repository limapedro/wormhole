# ROADMAP — Wormhole

Phased plan. Each phase maps to a GitHub milestone. Issue links are filled in
once the milestones and issues are created from this document after the docs
PR merges (each phase below has an "Issue map" with links to its GitHub issues).

Work follows the loop in the standing contract: read
[PROGRESS.md](PROGRESS.md), pick the highest-priority open issue, experiment
first when the task involves measurement or design, work on a short-lived
branch, record findings, update docs, open a PR, merge.

## Phase 0 — Setup (milestone `0 - Setup`)

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

**Issue map (milestone `0 - Setup`):** [#2](https://github.com/limapedro/wormhole/issues/2) (repo created, closed), [#3](https://github.com/limapedro/wormhole/issues/3) (docs PR, closed), [#4](https://github.com/limapedro/wormhole/issues/4) (issue setup, closed), [#5](https://github.com/limapedro/wormhole/issues/5) (CI skeleton).

## Phase 1 — Reconnaissance (milestone `1 - Recon`)

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

**Issue map (milestone `1 - Recon`):** [#6](https://github.com/limapedro/wormhole/issues/6) (Go toolchain), [#7](https://github.com/limapedro/wormhole/issues/7) (clone + pin hash), [#8](https://github.com/limapedro/wormhole/issues/8) (license check), [#9](https://github.com/limapedro/wormhole/issues/9) (codebase inventory), [#10](https://github.com/limapedro/wormhole/issues/10) (Windows build), [#11](https://github.com/limapedro/wormhole/issues/11) (exp_000), [#12](https://github.com/limapedro/wormhole/issues/12) (hypotheses).

## Phase 2 — Harness (milestone `2 - Harness`)

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

**Dependencies:** Phase 1. **Issue map (milestone `2 - Harness`):** [#13](https://github.com/limapedro/wormhole/issues/13) through [#24](https://github.com/limapedro/wormhole/issues/24) — proxy, dissector, instrumented build, mock OBS host, video/audio generators, reference peers, replay, differential tester, benchmark runner, protocol questions and Windows capture backend decision.

## Phase 3 — Architecture (milestone `3 - Architecture`)

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

**Dependencies:** Phase 2. **Issue map (milestone `3 - Architecture`):** [#25](https://github.com/limapedro/wormhole/issues/25) (C+Rust decision), [#26](https://github.com/limapedro/wormhole/issues/26) (buffers), [#27](https://github.com/limapedro/wormhole/issues/27) (SIMD JPEG), [#28](https://github.com/limapedro/wormhole/issues/28) (I/O model), [#29](https://github.com/limapedro/wormhole/issues/29) (threading rule), [#30](https://github.com/limapedro/wormhole/issues/30) (v2 spec).

## Phase 4 — Parity, Windows first (milestone `4 - Parity`)

**Goal:** full Teleport feature parity in compatibility mode on Windows.

- [ ] Sender + receiver in compatibility mode.
- [ ] Discovery.
- [ ] Source / filter / output equivalents.
- [ ] Settings UI.
- [ ] Full parity checklist green.
- [ ] Differential tests passing against original Teleport.

**Acceptance criteria:** all P01–P24 rows `verified`; differential tester
green; CI green on Windows.

**Dependencies:** Phases 1–3. **Issue map (milestone `4 - Parity`):** [#31](https://github.com/limapedro/wormhole/issues/31) (sender), [#32](https://github.com/limapedro/wormhole/issues/32) (receiver), [#33](https://github.com/limapedro/wormhole/issues/33) (discovery), [#34](https://github.com/limapedro/wormhole/issues/34) (source/filter/output), [#35](https://github.com/limapedro/wormhole/issues/35) (settings UI), [#36](https://github.com/limapedro/wormhole/issues/36) (checklist + differential tests).

## Phase 5 — Efficiency (milestone `5 - Efficiency`)

**Goal:** profile-driven optimization until every target in
[PERFORMANCE_TARGETS.md](PERFORMANCE_TARGETS.md) is met.

- Every optimization is its own experiment with `exp_NNN/HYPOTHESIS.md` and
  `FINDINGS.md`.
- Focus areas: buffer pools / ring buffers, allocation elimination, SIMD
  codec, socket tuning, backpressure policy, framing overhead.

**Acceptance criteria:** targets met, each claim linked to a committed
benchmark result; no OBS render-thread stalls attributable to the plugin.

**Dependencies:** Phase 4. **Issue map (milestone `5 - Efficiency`):** [#37](https://github.com/limapedro/wormhole/issues/37) (profile), [#38](https://github.com/limapedro/wormhole/issues/38) (allocations), [#39](https://github.com/limapedro/wormhole/issues/39) (codec), [#40](https://github.com/limapedro/wormhole/issues/40) (framing/socket), [#41](https://github.com/limapedro/wormhole/issues/41) (verify targets).

## Phase 6 — Multi-audio (milestone `6 - Multi-audio`)

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

**Dependencies:** Phase 4 (and ideally Phase 5). **Issue map (milestone `6 - Multi-audio`):** [#42](https://github.com/limapedro/wormhole/issues/42) (OBS mapping), [#43](https://github.com/limapedro/wormhole/issues/43) (sender), [#44](https://github.com/limapedro/wormhole/issues/44) (receiver), [#45](https://github.com/limapedro/wormhole/issues/45) (protocol v2 descriptors), [#46](https://github.com/limapedro/wormhole/issues/46) (sync/robustness), [#47](https://github.com/limapedro/wormhole/issues/47) (tests).

## Phase 7 — Linux and macOS (milestone `7 - Platforms`)

- [ ] Ports and platform-specific audio/video paths.
- [ ] CI green on `windows-latest`, `ubuntu-latest`, `macos-latest`, and Apple
      Silicon runner if available.
- [ ] Installers: Windows installer, Linux package/tarball, macOS signed and
      notarized bundle; notarization steps in [PLATFORMS.md](PLATFORMS.md).

**Acceptance criteria:** all platform notes accurate; anything unverifiable
on real hardware flagged in [RISKS.md](RISKS.md).

**Dependencies:** Phase 4; parallel with 5/6 where possible. **Issue map (milestone `7 - Platforms`):** [#48](https://github.com/limapedro/wormhole/issues/48) (Linux), [#49](https://github.com/limapedro/wormhole/issues/49) (macOS), [#50](https://github.com/limapedro/wormhole/issues/50) (Windows installer), [#51](https://github.com/limapedro/wormhole/issues/51) (Linux package), [#52](https://github.com/limapedro/wormhole/issues/52) (macOS signing/notarization).

## Phase 8 — Hardening (milestone `8 - Hardening`)

- Fuzz every network-input parser; 24h+ soak; chaos (loss, reorder, delay,
  bandwidth caps, abrupt disconnects); leak checks; crash recovery.

**Acceptance criteria:** no crash/freeze under any harness-generated failure;
all sanitizer/soak tests pass. **Dependencies:** Phases 4–7.

**Issue map (milestone `8 - Hardening`):** [#53](https://github.com/limapedro/wormhole/issues/53) (fuzzing), [#54](https://github.com/limapedro/wormhole/issues/54) (soak), [#55](https://github.com/limapedro/wormhole/issues/55) (chaos), [#56](https://github.com/limapedro/wormhole/issues/56) (leaks/crash recovery).

## Phase 9 — Release (milestone `9 - Release`)

- Semantic-version releases, CI-built artifacts for all three platforms with
  checksums; release notes from [PROGRESS.md](PROGRESS.md); user docs,
  migration guide, upstream attribution.

**Acceptance criteria:** `v0.1.0` and later tagged; artifacts attached;
docs complete. **Dependencies:** Phase 8.

**Issue map (milestone `9 - Release`):** [#57](https://github.com/limapedro/wormhole/issues/57) (release artifacts), [#58](https://github.com/limapedro/wormhole/issues/58) (user docs/migration/attribution).
