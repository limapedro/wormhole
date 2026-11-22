# PROGRESS — Wormhole (living dashboard)

Newest entries first. Updated on every merge to `main`.

## Current status

| Field | Value |
|---|---|
| Current phase | Phase 2 — Harness |
| Overall completion | ~40% (setup + recon + harness foundation; no plugin code yet) |
| Buildable `main` | Yes (docs + Rust workspace; CI green locally) |
| CI | Configured on `windows-latest`, `ubuntu-latest`, `macos-latest` (issue #5 done) |
| Blocked items | No local C compiler/libobs SDK headers → source build and instrumented build blocked (issues #6/#15, RISKS.md R13). `exp_000` baseline not yet run. |
| Latest benchmark | **No product benchmark.** Harness self-benchmark only (exp_001, explicitly not a Teleport/Wormhole result) |
| Next three actions | 1. Capture real Teleport 0.8.0 traffic through the proxy and confirm `C`-class protocol facts (#13/#14/#23, `exp_002`). 2. Extend media generator to full resolution/fps matrix + multi-track (#17/#18). 3. Resolve portable toolchain blocker for the instrumented build (#6/#15). |

## Phase completion

| Phase | Milestone | Progress | Notes |
|---|---|---|---|
| 0 — Setup | `0 - Setup` | 100% | Repo, docs, milestones, labels, issues and CI skeleton (#5) complete |
| 1 — Recon | `1 - Recon` | 75% | Upstream cloned/pinned, license decision logged, full source-derived protocol and parity checklist done; source build and exp_000 baseline remain blocked/open |
| 2 — Harness | `2 - Harness` | 35% | `wh-protocol` + typed TOML CLI, dissector, basic proxy, media/synth/replay/sink and self-bench implemented; exp_001 fidelity confirmed; instrumented build, pcap timestamps, multi-track media, differential tester and full benchmark runner pending |
| 3 — Architecture | `3 - Architecture` | 0% | Starting defaults drafted in `ARCHITECTURE_LOG.md`, unconfirmed |
| 4 — Parity | `4 - Parity` | 0% | Not started |
| 5 — Efficiency | `5 - Efficiency` | 0% | Not started |
| 6 — Multi-audio | `6 - Multi-audio` | 0% | Not started |
| 7 — Platforms | `7 - Platforms` | 0% | Not started |
| 8 — Hardening | `8 - Hardening` | 0% | Not started |
| 9 — Release | `9 - Release` | 0% | Not started |

## Benchmark dashboard

Targets are starting hypotheses from the standing contract and will be
replaced after `exp_000` measures real Teleport baselines. **No performance
claim appears here without a linked committed result file.** As of now there
are no measured numbers.

| Metric | Scenario | Teleport baseline | Wormhole target | Evidence |
|---|---|---|---|---|
| CPU (sender+receiver) | 1080p30 | not measured | ≥50% lower | `exp_000` (pending) |
| CPU (sender+receiver) | 1080p60 | not measured | ≥50% lower | `exp_000` (pending) |
| CPU (sender+receiver) | 1440p60 | not measured | ≥50% lower | `exp_000` (pending) |
| CPU (sender+receiver) | 4K30 | not measured | ≥50% lower | `exp_000` (pending) |
| Resident memory | 1080p30 | not measured | ≥50% lower | `exp_000` (pending) |
| Steady-state heap allocations | all | not measured | zero on hot path | `exp_000` (pending) |
| Glass-to-glass latency (mean/p99) | 1080p60 | not measured | ≤1 frame additional buffering | `exp_000` (pending) |
| Frame pacing (jitter/drops/dupes) | 1080p60 | not measured | no worse than baseline | `exp_000` (pending) |
| Bandwidth at equal quality | 1080p60 | not measured | measured, then target set | `exp_000` (pending) |
| Startup / reconnect time | 1080p60 | not measured | measured, then target set | `exp_000` (pending) |
| A/V sync error | multi-track | not measured | <10 ms all tracks | `exp_000` (pending) |
| Network failure behavior | chaos suite | not measured | graceful degradation | Phase 8 |

## Log

### 2026-11-22 — First code: Rust workspace, CI and harness foundation (Phase 2)

- Added the first code commit: `crates/protocol/` (`wh-protocol`) with strict,
  bounded, byte-accurate decode/encode for the Teleport wire format, and
  `harness/` (`wormhole-harness`) with typed TOML config structs and only
  `--config` / `--help` flags. Added `Cargo.toml` workspace and
  `.github/workflows/ci.yml` build/test matrix (Windows, Linux, macOS) with
  fmt, clippy `-D warnings`, tests and release build — closing issue #5.
- Harness commands implemented: `check`, `synth`, `dissect`, `media`, `proxy`,
  `replay`, `sink`, `bench` (the last is a **harness self-benchmark**, not a
  product benchmark and not cited by any performance claim).
- `exp_001/HYPOTHESIS.md` written before the formal run;
  [exp_001/FINDINGS.md](exp_001/FINDINGS.md) records the result: 120 synthetic
  packets, 184,800 bytes, decode→encode byte-identical; proxy capture and sink
  file share SHA-256 `75487ADA…DDC` with the fixture; 15 unit tests pass.
  A first run failed because `replay` targeted the sink instead of the proxy;
  fixed and retested.
- Local verification: `cargo fmt --all -- --check`,
  `cargo clippy --workspace --all-targets -- -D warnings`,
  `cargo test --workspace --all-targets` all pass.
- Still missing: pcap export and high-resolution timestamps in the proxy,
  full media matrix/multi-track, instrumented upstream build, mock OBS host,
  reference peers, differential tester and S1–S10 benchmark runner.

### 2026-11-22 — Phase 1 reconnaissance complete (source-derived)

- Cloned upstream `fzwoch/obs-teleport` into git-ignored `third_party/` and
  pinned HEAD `e8c49f4be390ed17c434099541ce2fd65720e4c8` and release tag
  `0.8.0` `b61ee3c63f629bca75e7571bb7bdd151d8ee5aa6`. Source is identical
  between them (`git diff 0.8.0 HEAD` changes only CI config). Read all
  ~2,400 lines; results are in [PROTOCOL.md](PROTOCOL.md) with line-level
  citations.
- **License check:** upstream is GPL-2.0-or-later; libobs is
  GPL-2.0-or-later; Wormhole is now GPL-2.0-or-later with LICENSE finalized
  and decision logged in [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md).
  RISKS.md R01 resolved. No upstream code was copied.
- **Hypotheses overturned by evidence:** discovery is UDP multicast
  (`239.255.255.250:9999`, raw JSON) not mDNS; audio is uncompressed
  interleaved PCM, not Opus; there is no wire handshake, version or
  capability exchange; video is JPEG via SIMD libjpeg-turbo with quality
  1–100 (default 90).
- Parity checklist in [GOAL.md](GOAL.md): all 24 rows moved from
  `unverified` to `confirmed` (source evidence). Differential-test
  `verified` status is still pending Phase 4.
- **Negative result:** building upstream from source on Windows is blocked —
  no Go, no C compiler, no libobs SDK headers locally; upstream CI only builds
  via a Linux Docker/mingw toolchain. Recorded in [HARNESS.md](HARNESS.md),
  RISKS.md R13, issues #6/#15. The official 0.8.0 release DLL is used for
  recon instead (SHA-256 `7BC1412C…E45C6`).
- **Incident:** the NSIS installer ignored `/D` and wrote to the pre-existing
  global OBS plugin directory; documented in HARNESS.md and RISKS.md R14;
  no user profile data changed and the install was left intact.
- No benchmark was run; `exp_000/FINDINGS.md` does not exist yet.

### 2026-11-22 — Docs PR merged and issue tracker created (Phase 0)

- PR [#1](https://github.com/limapedro/wormhole/pull/1) (docs-only) merged to
  `main` as `c7e473f` (squash). It contained only the section-3 planning
  documents and `exp_000/HYPOTHESIS.md` — no code, satisfying the contract's
  first-branch rule.
- Created 12 labels (`protocol`, `video`, `audio`, `multi-audio`, `perf`,
  `platform:windows`, `platform:linux`, `platform:macos`, `harness`, `docs`,
  `blocked`, `research`), 10 milestones (`0 - Setup` … `9 - Release`) and
  57 issues, one per roadmap item. Issues #2–#4 are closed as completed;
  #5 (CI skeleton) remains open for the first code commit.
- [ROADMAP.md](ROADMAP.md) now links every phase to its GitHub issues.

### 2026-11-22 — Repository created (Phase 0)

- Verified `gh` access: account `limapedro`, scopes `gist`, `read:org`,
  `repo`, `workflow`.
- Confirmed `limapedro/wormhole` name was free; created **public** repo.
- Initial commit `eeabbb8`: README, comprehensive `.gitignore`, placeholder
  LICENSE (license finalized after Phase 1 check).
- Dev machine recorded: Intel i5-12400F (6C/12T), 32 GB RAM, Windows 11 Pro
  build 26300, Rust 1.99, CMake 4.3.3, OBS Studio 33.0.0 at
  `C:\Program Files\obs-studio`, NVIDIA GTX 1660 SUPER. **Go not installed.**
- Opened docs-only PR `docs/planning-documents` containing the section-3
  planning docs.
