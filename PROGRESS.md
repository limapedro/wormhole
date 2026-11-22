# PROGRESS — Wormhole (living dashboard)

Newest entries first. Updated on every merge to `main`.

## Current status

| Field | Value |
|---|---|
| Current phase | Phase 2 — Harness |
| Overall completion | ~40% (setup + recon + harness foundation; no plugin code yet) |
| Buildable `main` | Yes (all-C core and harness; CI green on Windows/Linux/macOS) |
| CI | Configured on `windows-latest`, `ubuntu-latest`, `macos-latest` (issue #5 done) |
| Blocked items | No local C compiler/libobs SDK headers → source build and instrumented build blocked (issues #6/#15, RISKS.md R13). `exp_000` baseline not yet run. |
| Latest benchmark | **No product benchmark.** Harness self-benchmark only (exp_001, explicitly not a Teleport/Wormhole result) |
| Next three actions | 1. Capture real Teleport 0.8.0 traffic through the proxy and confirm `C`-class protocol facts (#13/#14/#23, `exp_002`). 2. Extend media generator to full resolution/fps matrix + multi-track (#17/#18). 3. Resolve portable toolchain blocker for the instrumented build (#6/#15). |

## Phase completion

| Phase | Milestone | Progress | Notes |
|---|---|---|---|
| 0 — Setup | `0 - Setup` | 100% | Repo, docs, milestones, labels, issues and CI skeleton (#5) complete |
| 1 — Recon | `1 - Recon` | 75% | Upstream cloned/pinned, license decision logged, full source-derived protocol and parity checklist done; source build and exp_000 baseline remain blocked/open |
| 2 — Harness | `2 - Harness` | 75% | All-C harness: typed TOML config, check/synth/dissect/proxy/quality/differential/media/reference peers; two-portable-OBS monitor; exp_001–exp_010 confirmed; instrumented build, pcap timestamps, mock OBS host, live differential driving and full S1–S10 runner pending |
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

### 2026-11-22 — OBS launch blockers removed; OBS-MCP sub-project created

- `scripts/setup_portable_obs.ps1` now compiles `scripts/obs_plugin_stub.c`
  with MSVC and replaces the six bundled modules that abort OBS 33 startup
  (AJA, DeckLink, multi-source-record, obs-shaderfilter) in both portable
  copies, and enables obs-websocket 5 through its JSON config. Verified with a
  Win32 window-title enumeration: **zero `Plugin Errors` dialogs**, Teleport
  0.8.0 loads, scenes load normally.
- New independent public repository **limapedro/OBS-MCP** (all C, MIT, 10 MCP
  tools, OBS WebSocket v5 client, CPU/log sampling, two-portable-OBS control).
  Its `docs/VERIFICATION.md` records a full MCP session: both OBS instances
  launched, `GetVersion` succeeded on both (OBS 33.0.0, obs-websocket 5.7.6),
  CPU 0.094 s / 0.78 % / 257.7 MB sampled, logs tailed, both terminated. CI
  green on Windows/Linux/macOS.

### 2026-11-22 — Rust workspace removed; repository is all C

- Following the user directive and the evidence in `exp_005`–`exp_010`, the
  Rust workspace (`Cargo.toml`, `Cargo.lock`, `crates/`, `harness/src/`) was
  deleted. All harness commands now exist in C with parity demonstrated on
  real Teleport captures.
- CI now runs only the C build/test matrix (Windows MSVC, Linux and macOS cc)
  with warnings as errors. HARNESS.md, AGENTS.md, CONTRIBUTING.md,
  PLATFORMS.md, PERFORMANCE_TARGETS.md and the status dashboard were updated.
- Historical experiment documents that mention Rust are retained unchanged as
  evidence of the port; they describe past implementations, not current code.

### 2026-11-22 — All-C quality and differential commands (Phase 2)

- Vendored public-domain `stb_image.h` under `vendor/stb/` with license note.
- Added C `quality` (JPEG decode, mean RGB, MSE, PSNR) and C `differential`
  (packet count/type/timestamp/payload-hash comparison with JSON report)
  commands; build scripts include `-Ivendor` and `-lm` on POSIX.
- [exp_009/FINDINGS.md](exp_009/FINDINGS.md) (hypothesis first): MSVC `/W4 /WX`
  build clean; C quality on the live capture matched Rust exactly (309 frames,
  1280x720, RGB (18,1,230), MSE 0.000, capped PSNR 120 dB); C differential on
  identical captures reported 790/790 compared, 0 differences.
- The complete OBS monitoring analysis path is now all C except the PowerShell
  launcher. Remaining Rust-only commands: `media`, `ref-sender`, `ref-receiver`.

### 2026-11-22 — All-C proxy captures the live OBS pair (Phase 2)

- Ported the TCP proxy to C (`core/c/proxy.c`, Winsock/POSIX via `select()`),
  with both-direction capture, delay, deterministic drops and a bandwidth cap.
  `scripts/run_two_obs.ps1` now accepts `-Harness` to choose the C binary.
- [exp_008/FINDINGS.md](exp_008/FINDINGS.md) (hypothesis first): C proxy ran
  the two-OBS session; capture 8,632,459 bytes decoded by the C dissector into
  790 packets (309 JPEG + 481 WAVE), byte-identical; quality report decoded
  309 frames at 1280x720. CPU: receiver 0.594 s (0.82 % of 12 logical CPUs),
  sender 0.844 s (1.17 %) over 6.03 s. MSVC `uint64_t` conversion warning
  under `/WX` was found and fixed (recorded).
- The full capture path — proxy → capture → dissect — is now all C on the real
  Teleport stream. Remaining Rust-only commands are temporary.

### 2026-11-22 — All-C harness: check/synth/dissect (Phase 2)

- Added `include/wormhole/harness.h`, `core/c/harness.c` (typed TOML-subset
  config parser and commands) and `core/c/harness_main.c` (only `--config` and
  `--help`). MSVC build script now compiles the C harness too.
- [exp_007/FINDINGS.md](exp_007/FINDINGS.md) (hypothesis first): MSVC `/W4 /WX`
  build clean, C tests pass; C dissector on the live OBS capture reported
  **768 packets / 8,389,332 bytes / byte-identical**, matching the Rust
  dissector exactly; C synth→dissect produced 120 packets / 9,600 bytes.
  A wrong cumulative-length assumption in the first implementation was found
  and fixed (recorded).
- Remaining Rust-only harness commands (proxy, quality, differential, media,
  reference peers) are explicitly temporary pending C ports.

### 2026-11-22 — Two portable OBS instances monitored (Phase 2)

- Built two self-contained portable OBS copies under git-ignored
  `third_party/obs-portable/` and added repeatable scripts
  `scripts/setup_portable_obs.ps1` and `scripts/run_two_obs.ps1`.
- [exp_006/FINDINGS.md](exp_006/FINDINGS.md) (hypothesis first): Teleport 0.8.0
  loaded in both instances; live proxy capture 8,389,332 bytes = 768 packets
  (300 JPEG + 468 WAVE), byte-identical round-trip, 48 kHz/float/2-ch audio,
  ~10 s span. CPU over 6.02 s: receiver 0.453 s (0.63 % of 12 logical CPUs),
  sender 1.125 s (1.56 %); working sets 266.1/275.5 MB. Image quality over
  300 decoded 1280x720 frames: mean RGB (18,1,230), MSE 0.000, PSNR capped
  at 120 dB.
- The user's "plugin errors" screenshot was diagnosed from logs: it is caused
  by bundled `aja`/`decklink`/`multi-source-record` modules, not Teleport,
  which loaded and streamed. No upstream patch needed.
- Harness gained `capture_down` (records both TCP directions) and a `quality`
  command. Negative results recorded: initial empty capture due to recording
  only one direction; removing unrelated core modules blocked OBS startup;
  infinite PSNR serialized as `null` (now capped at 120 dB).
- These are **Teleport** baseline numbers, not Wormhole results; no parity or
  performance claim follows yet.

### 2026-11-22 — All-C pivot: Visual Studio core builds and is wire-compatible

- User directed an all-C implementation; previous Rust architecture default
  overturned in [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md) with the directive,
  toolchain evidence and consequences recorded.
- Added `include/wormhole/protocol.h`, `core/c/protocol.c`, `tests/test_protocol.c`
  and `tests/capture_writer.c`; `scripts/build_c.bat` uses `vswhere` +
  `vcvars64.bat` and `cl /W4 /WX`; `scripts/build_c.sh` covers POSIX. Added a
  `c-core` CI job on Windows/Linux/macOS.
- [exp_005/FINDINGS.md](exp_005/FINDINGS.md) (hypothesis first): MSVC 14.42
  compiled both targets with **zero warnings**; all C tests passed; a
  C-generated 8-packet / 640-byte capture was decoded and re-encoded
  byte-identically by the Rust dissector (SHA-256 `C782F8EE…9281`). First
  formal run failed on MSVC's `fopen` deprecation under `/WX`; fixed and
  recorded.
- Rust harness remains temporarily; migration to C is the next architecture
  task. No performance claims made.

### 2026-11-22 — Differential capture comparison (Phase 2)

- Added a `differential` command that decodes two captures and compares
  message type, timestamp and payload hash, writing a JSON report and failing
  on mismatch. Stage 1 of issue #21.
- [exp_004/FINDINGS.md](exp_004/FINDINGS.md) (hypothesis first): identical
  captures reported `identical=true` with exit 0; flipping one timestamp byte
  produced `packet 0: timestamp 0 vs 1` with exit 1. 19 tests pass; fmt/clippy
  clean. No parity claim is made — no real Teleport vs Wormhole comparison has
  occurred.

### 2026-11-22 — Full media matrix and multi-track generator (Phase 2)

- Extended `media` to produce the five `PERFORMANCE_TARGETS.md` profiles
  (720p30, 1080p30, 1080p60, 1440p60, 4K30) and four independent WAV tracks
  (440/880/1320/1760 Hz with per-second sync markers).
- [exp_003/FINDINGS.md](exp_003/FINDINGS.md) (hypothesis written first): 11
  files per run, all I420 sizes matched `w*h*1.5*frames`, all WAV headers
  valid, and every file byte-identical across two runs. 17 tests pass;
  clippy/fmt clean after fixing two unused-variable warnings.

### 2026-11-22 — Reference sender/receiver implemented (Phase 2)

- Added `ref-sender` and `ref-receiver` commands using `jpeg-encoder` and
  `jpeg-decoder`: standalone Teleport-protocol peers with no libobs/Go
  dependency, closing issue #19.
- [exp_002/FINDINGS.md](exp_002/FINDINGS.md) (hypothesis written first):
  10 frames at 1280x720/30 fps through the proxy produced 20 packets
  (10 JPEG + 10 WAVE), 10 successful JPEG decodes, 0 failures, 10 unique
  timestamps spanning 0–300 ms; proxy capture 359,903 bytes decoded and
  re-encoded byte-identically (SHA-256 `5125EBC1…0254`). Two independent runs
  produced identical hashes. A first determinism attempt failed because the
  rewritten config did not update the proxy listen port; recorded as a
  negative result.
- CI for PR #61: Windows and Linux passed; the first macOS attempt failed on a
  transient `codeload.github.com` DNS error while downloading an action
  (runner infrastructure, not code) and passed after a rerun. All three jobs
  green before merge.

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
