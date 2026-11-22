# PROGRESS — Wormhole (living dashboard)

Newest entries first. Updated on every merge to `main`.

## Current status

| Field | Value |
|---|---|
| Current phase | Phase 0 — Setup |
| Overall completion | ~15% (setup in progress; no plugin code yet) |
| Buildable `main` | Yes (docs only) |
| CI | Not yet configured (skeleton lands with first code commit) |
| Blocked items | Go toolchain missing locally (see [RISKS.md](RISKS.md)) |
| Latest benchmark | None yet — baseline pending `exp_000` |
| Next three actions | 1. Merge docs PR → create issues/milestones. 2. Start Phase 1 recon (clone upstream, license check, build). 3. Run `exp_000` baseline. |

## Phase completion

| Phase | Milestone | Progress | Notes |
|---|---|---|---|
| 0 — Setup | `0 — Setup` | 60% | Repo + initial commit done; docs PR open; issues/milestones pending |
| 1 — Recon | `1 — Recon` | 5% | `exp_000/HYPOTHESIS.md` written; upstream not yet cloned |
| 2 — Harness | `2 — Harness` | 0% | Not started |
| 3 — Architecture | `3 — Architecture` | 0% | Starting defaults drafted in `ARCHITECTURE_LOG.md`, unconfirmed |
| 4 — Parity | `4 — Parity` | 0% | Not started |
| 5 — Efficiency | `5 — Efficiency` | 0% | Not started |
| 6 — Multi-audio | `6 — Multi-audio` | 0% | Not started |
| 7 — Platforms | `7 — Platforms` | 0% | Not started |
| 8 — Hardening | `8 — Hardening` | 0% | Not started |
| 9 — Release | `9 — Release` | 0% | Not started |

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
