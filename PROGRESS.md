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

### 2026-11-22 — per-channel audio sources with own thread/connection (exp_019)

- Added `wormhole-audio-output` (gaming PC) and `wormhole-audio` (streaming PC):
  one OBS audio source per network channel, each with its own thread, bounded
  queue and TCP connection. Add as many as needed.
- Video remains one composited signal on port 18090; audio channels use
  user-chosen ports (default 18091+) and can be assigned to separate OBS
  recording tracks for editing.
- Both audio panels got guided cyberpunk UI with discovered-node selection.
- Fixed NV12 capture crash, missing frame dimensions, refcount bug, silent
  recordings and periodic audio gaps. See
  [exp_019/FINDINGS.md](exp_019/FINDINGS.md).

### 2026-11-22 — v0.2 scene mirror, output-track audio, clean Tools UX (exp_018)

- One composited video signal mirrored into `WH <scene>` scenes; the mirrored
  video source is renamed to the sender's source name (`Main Monitor`).
- Audio uses OBS output tracks (`raw_audio2`, default tracks 1–2, configurable
  1–6), mapped to the sender's source names on the receiver. Mute/visibility/
  active-scene changes propagate via a periodic versioned state manifest.
- Mirroring is non-destructive: the streaming PC's mic/webcam are preserved and
  copied into mirrored scenes (verified with `STREAM-PC MIC`).
- Tools menu split into `Host Game PC` / `Mirror Game PC` entries.
- exp_017 records the superseded per-source attempt; see
  [exp_018/FINDINGS.md](exp_018/FINDINGS.md).

### 2026-11-22 — Crash-free disable and unlocked OBS output settings (exp_016)

- Fixed an OBS crash when disabling Wormhole: the stop callback ran twice and
  joined/closed an already-freed thread handle. Stop is now idempotent and
  called unconditionally from the control source.
- Fixed a shutdown hang: `shutdown()` before `close()` interrupts blocking
  sends, and `obs_output_begin_data_capture()` no longer runs on a foreign
  thread.
- Fixed OBS Settings → Output Mode staying greyed out: the output now calls
  `obs_output_end_data_capture()` on stop, so `obs_video_active()` returns to 0
  when Wormhole is disabled (verified 1 → 0 → 1, Mode changed to Advanced).
- Receiver now reconnects automatically when the sender restarts.
- Evidence in [exp_016/FINDINGS.md](exp_016/FINDINGS.md).

### 2026-11-22 — Friendlier UX: sender identity, LAN discovery, cyberpunk UI (exp_015)

- Sender can be **renamed**: a new `IDENTIFIER` field (default from hostname,
  test value `GAMING-PC-NEO`) is broadcast over UDP and carried in the v2
  HELLO_ACK, so the receiver logs `v2 linked to sender 'GAMING-PC-NEO'`.
- Receiver **auto-discovers** Wormhole nodes on UDP 18092 (verified: discovered
  `GAMING-PC-NEO` at `192.168.1.104:18090`) and offers both a discovered-node
  combo box and manual host/port fields, with a `SCAN NETWORK` refresh button.
- Both settings panels restyled with a pure-C **cyberpunk / terminal**
  aesthetic: box-drawn banners, numbered sections, `>` controls, and a sender
  diagnostics button (verified via its log output). No Qt/C++ introduced.
- Fixed stream descriptors being re-sent on every audio packet; now once per
  track. Build flags hardened with `/DWIN32_LEAN_AND_MEAN` for winsock.
- Evidence in [exp_015/FINDINGS.md](exp_015/FINDINGS.md).

### 2026-11-22 — Wormhole plugin runs end to end on both OBS instances (exp_014)

- Built the real plugin (`wormhole.dll`, C + SIMD libjpeg-turbo 3.2.0) with
  Teleport-style Tools menu, sender output, receiver source, v2 negotiation,
  dynamic multi-audio from OBS's mixers (no user "Audio Tracks" setting), and
  bounded queues that keep network work off OBS threads.
- `scripts/setup_portable_obs.ps1` now removes Teleport entirely and configures
  the gaming PC with its primary 2560×1440 monitor + three audio sources and
  the streaming PC with one Wormhole receiver source.
- Verified with both instances launched: Tools menu installed on both, no
  Teleport in logs, v2 negotiated, 6 audio track descriptors, both previews
  showing the same desktop with correct colors (user-confirmed). OBS CPU
  ~1.5–3 % per instance. Evidence in [exp_014/FINDINGS.md](exp_014/FINDINGS.md).
- Fixed eight defects found during integration, including stack overflow from
  4 MB stack buffers, `bzalloc`/`free` heap corruption, a `/MT` vs `/MD` CRT
  mismatch, absolute timestamps and a wrong color matrix. All recorded.

### 2026-11-22 — Wormhole plugin foundation and protocol v2 (exp_013)

- Honest status: the Wormhole plugin did **not** exist before this work; only
  recon/harness/MCP did. Added `plugin/src/plugin.c`, which registers
  `wormhole-source` and `wormhole-output`, and `scripts/build_plugin.bat`.
- Built `wormhole.dll` against OBS 33.0.0 headers with an import library
  generated from `obs.dll` (`scripts/make_obs_import_lib.ps1`, 1,781 exports).
  OBS log after loading: `[wormhole] version 0.1.0`, `wormhole.dll` listed as
  loaded. Plugin load path verified.
- Added `docs/WORMHOLE_V2.md`, `include/wormhole/protocol_v2.h`,
  `core/c/protocol_v2.c` and two test programs. v2 has versioned framing,
  total length, sequence numbers and nanosecond timestamps; HELLO/ACK
  negotiation, named/clocked stream descriptors for multiple audio tracks,
  and video/audio frames. Both unit tests and a real TCP negotiation test pass
  (3 descriptors, 3 audio frames, one 1920x1080 video frame).
- **Not yet complete**: the plugin's source/output callbacks are stubs — no
  real media path, settings UI, JPEG conversion or full compatibility mode.
  See [exp_013/FINDINGS.md](exp_013/FINDINGS.md).

### 2026-11-22 — Real 60-second 1080p60 NVENC recording accepted (exp_012)

- Configured the streaming PC for Advanced recording: `obs_nvenc_h264_tex`,
  CBR 8000 kbit/s, preset **p4**, profile high, MP4, 1920x1080, AAC 160 kbit/s.
- Verified the gaming PC Display Capture source is the **primary monitor**
  (`monitor=0`, `capture_cursor=true`) and active; sender 2560x1440 → 1920x1080
  at 60 fps, receiver 1920x1080 at 60 fps.
- Recorded for a full 60 s through OBS-MCP. Final file
  `exp_results/recordings/2026-11-22 03-51-46.mp4` (git-ignored): **59.983 s**,
  H.264 High, 1920x1080, 60 fps, video bitrate **8,009,512 bit/s**,
  AAC 159,888 bit/s, 61.3 MB. OBS log confirms NVENC CBR 8000, preset p4,
  high profile, 1920x1080.
- CPU: sender 3.97–5.47 %, receiver 1.82–4.95 %; receiver `GetStats` 60 fps,
  4.42 % CPU, 425.7 MB. Details in [exp_012/FINDINGS.md](exp_012/FINDINGS.md).
- Negative results recorded: two earlier takes were 57.9 s and 64.0 s (sampling
  inside the window), wrong `GetInputSettings` field name, and an empty wire
  capture for the final run.

### 2026-11-22 — 1440p gaming PC → 1080p streaming PC workflow (exp_011)

- Configured the sender profile for 2560x1440 base → 1920x1080 output at 60
  fps (matching the physical 2560x1440 display), the receiver for 1920x1080/60,
  and added a 40 Mbit/s capped proxy config
  (`harness/config/obs-proxy-40m.toml`).
- Through OBS-MCP, launched both instances, confirmed video settings via
  WebSocket, created/confirmed a Display Capture source (`monitor_capture`
  in OBS 33), DesktopAudio, MicOne and MicTwo, and verified all four active.
- Clean 12 s capture: 1,427 packets (802 JPEG + 625 WAVE), 34,282,274 bytes in
  13.35 s = **20.54 Mbit/s** (below the 40 Mbit/s cap), byte-identical
  round-trip; all 802 frames decoded at 1920x1080. OBS stats: 60 fps, 0
  skipped render frames; sender CPU 4.27 % / 354.8 MB, receiver 2.84 % /
  304.2 MB. Details in [exp_011/FINDINGS.md](exp_011/FINDINGS.md).
- Fixed two bugs found during the run: wrong Display Capture source kind, and
  OBS-MCP's WebSocket client not consuming the `Identified` reply (breaking
  `GetStats`). OBS-MCP repo updated and verified.

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
