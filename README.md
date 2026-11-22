# Wormhole

Wormhole is a ground-up, high-efficiency rewrite of the **Teleport** OBS plugin
(obs-teleport, originally written in Go). It sends OBS video and audio from one
machine, or one OBS instance, to another over the network with the lowest
practical latency, CPU use, memory use and overhead.

Wormhole keeps everything Teleport does, adds multiple simultaneous audio
outputs, and targets Windows, Linux and macOS.

> **Status:** The all-C `wormhole` plugin now loads in OBS 33.0.0, appears in
> the Tools menu, and streams video with multi-audio between two portable OBS
> instances (see [exp_014](exp_014/FINDINGS.md)). See [PROGRESS.md](PROGRESS.md)
> for the live dashboard.

## Why

Teleport proved that low-latency network video in OBS is useful, but it leaves
performance, memory use, and multi-track audio on the table. Wormhole is a
clean, measured rewrite: feature parity first, then efficiency, then
multi-audio.

## Priorities (in order)

1. Feature parity with Teleport.
2. Large, measured efficiency gains.
3. Multiple simultaneous audio outputs.

## Documentation

- [GOAL.md](GOAL.md) — mission, success definition, non-goals, parity checklist
- [ROADMAP.md](ROADMAP.md) — phased plan, milestones, acceptance criteria
- [PROGRESS.md](PROGRESS.md) — running log and dashboard (start here for status)
- [PERFORMANCE_TARGETS.md](PERFORMANCE_TARGETS.md) — metrics, methodology, targets
- [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md) — locked decisions and evidence
- [PROTOCOL.md](PROTOCOL.md) — reverse-engineered Teleport wire protocol
- [HARNESS.md](HARNESS.md) — reverse-engineering and test harness
- [PLATFORMS.md](PLATFORMS.md) — per-platform build and packaging notes
- [RISKS.md](RISKS.md) — risks and open questions
- [CONTRIBUTING.md](CONTRIBUTING.md) — GitHub workflow and conventions
- [AGENTS.md](AGENTS.md) — standing instructions for AI agents
- [exp_000/HYPOTHESIS.md](exp_000/HYPOTHESIS.md) — first experiment

## Supported platforms

| Platform | Status |
|---|---|
| Windows | Primary development platform |
| Linux | CI build and test |
| macOS | CI build and test (build only until real hardware is available) |

## License

Wormhole is licensed **GPL-2.0-or-later** ([LICENSE](LICENSE)), matching
upstream obs-teleport and the libobs copyleft obligations. The license check
is logged in [ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md).

Upstream attribution: **obs-teleport**, Copyright (C) 2021-2026 Florian
Zwoch, GPL-2.0-or-later. Wormhole is an independent reimplementation; the
reverse-engineered protocol is documented in [PROTOCOL.md](PROTOCOL.md).
