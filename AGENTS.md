# AGENTS — standing instructions for AI agents in Wormhole

Read the root documents before acting: [GOAL.md](GOAL.md),
[ROADMAP.md](ROADMAP.md), [PROGRESS.md](PROGRESS.md),
[PERFORMANCE_TARGETS.md](PERFORMANCE_TARGETS.md),
[ARCHITECTURE_LOG.md](ARCHITECTURE_LOG.md), [PROTOCOL.md](PROTOCOL.md),
[HARNESS.md](HARNESS.md), [PLATFORMS.md](PLATFORMS.md),
[RISKS.md](RISKS.md), [CONTRIBUTING.md](CONTRIBUTING.md).

## Working loop

1. Read PROGRESS.md and ROADMAP.md; pick the highest-priority open issue.
2. If it involves measurement or a design question, write
   `exp_NNN/HYPOTHESIS.md` first.
3. Do the work on a short-lived branch.
4. Run tests/benchmarks; write `exp_NNN/FINDINGS.md` (including failures).
5. Update PROTOCOL.md, PERFORMANCE_TARGETS.md, ARCHITECTURE_LOG.md and
   PROGRESS.md as relevant, in the same commit series.
6. Open a PR linking the issue and evidence; CI must pass; merge.
7. Update the milestone; pick the next item.

## Key commands

- `gh auth status` — verify access.
- `git checkout -b feat/...|fix/...|exp/NNN-...|docs/...|perf/...`
- `cargo test`, `cargo fmt --check`, `cargo clippy -- -D warnings`
- `gh pr create`, `gh pr merge --squash`
- Harness tools take only `--config <file.toml>` and `--help` (command shapes
  in HARNESS.md; implemented in Phase 2).

## Configuration and evidence conventions

- Typed TOML config structs with explicit defaults; no flag sprawl.
- Every performance claim links to a committed result file in `exp_NNN/`.
- Teleport claims must be backed by evidence captured locally (code reading,
  traces, captures, differential tests) — prompt descriptions are hypotheses.
- Locked decisions only in ARCHITECTURE_LOG.md with date, decision,
  alternatives, evidence, consequences.

## Never do these

- Force-push `main` or rewrite published history.
- Commit secrets, tokens, personal paths or large binaries.
- Claim parity or a performance gain without linked evidence.
- Block OBS's render or audio threads from plugin code.
- Delete failed experiments or negative results.
- Modify files outside the Wormhole working directory, except installing into
  a dedicated test OBS profile.
- Publish upstream-derived code before the license check is logged.
- Guess Teleport behavior — measure it with the harness.
