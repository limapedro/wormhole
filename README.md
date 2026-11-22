# Wormhole

Wormhole is a ground-up, high-efficiency rewrite of the **Teleport** OBS plugin
(obs-teleport, originally written in Go). It sends OBS video and audio from one
machine, or one OBS instance, to another over the network with the lowest
practical latency, CPU use, memory use and overhead.

Wormhole keeps everything Teleport does, adds multiple simultaneous audio
outputs, and targets Windows, Linux and macOS.

> **Status:** Phase 0 (setup). This repository currently contains planning
> documentation only. See [PROGRESS.md](PROGRESS.md) for the live dashboard.

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

License selection is pending the Phase 1 license check. The current
[LICENSE](LICENSE) file is a **placeholder** and will be replaced once the
upstream license and copyleft obligations are verified and logged in
[ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md).
