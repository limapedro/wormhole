# CONTRIBUTING — Wormhole

## Workflow (from the standing contract)

- `main` is always buildable and has passing CI. Never force-push `main`,
  never rewrite published history.
- Work on short-lived branches: `feat/...`, `fix/...`, `exp/NNN-short-name`,
  `docs/...`, `perf/...`.
- Every merge to `main` goes through a PR, even for solo work. PR description
  states what changed, which roadmap item/issue it closes, and which
  experiment or benchmark backs it. Squash-merge unless history is clean and
  meaningful.
- Update [PROGRESS.md](PROGRESS.md) on every merge.
- Turn every roadmap item into a GitHub issue, grouped into milestones. Labels:
  `protocol`, `video`, `audio`, `multi-audio`, `perf`, `platform:windows`,
  `platform:linux`, `platform:macos`, `harness`, `docs`, `blocked`,
  `research`. Close issues only when acceptance criteria and linked evidence
  exist.
- CI from the first code commit: build/test on `windows-latest`,
  `ubuntu-latest`, `macos-latest` (and Apple Silicon if available); cache
  dependencies; fail on warnings; run formatter and linter checks.
- Releases: semantic versions from CI with checksums; notes from PROGRESS.md.

## Commit conventions

Small, atomic, imperative mood, typed prefix:
`feat:`, `fix:`, `perf:`, `docs:`, `test:`, `ci:`, `exp:`, `refactor:`.
Reference the issue number when one exists, e.g.
`feat(protocol): add v2 handshake negotiation (#42)`.

## Experiments

For any measurement or design question:

1. Create `exp_NNN/` with zero-padded sequential number.
2. Write `HYPOTHESIS.md` **before** running anything: what you expect, why,
   and what result would prove you wrong.
3. Run the experiment on a branch; write `FINDINGS.md` with raw numbers,
   what happened, and what you learned. Negative results get the same care.
   **Never delete a failed experiment.**
4. Link the experiment from PROGRESS.md and, where relevant, PROTOCOL.md /
   PERFORMANCE_TARGETS.md / ARCHITECTURE_LOG.md.
5. Locked decisions go in ARCHITECTURE_LOG.md with date, decision,
   alternatives, evidence, consequences. Reversal requires a new entry with
   new evidence.

## Configuration

Tools and harness programs take `--config path/to.toml`; only `--config` and
`--help` are allowed. Config lives in typed structs with explicit defaults.

## Tests and benchmarks

- Unit tests for framing, protocol parsing, buffers and clocks.
- Property-based + fuzz tests for every parser that reads network input.
- Integration tests through the mock OBS host and the differential tester.
- Soak (24h+) and chaos tests (loss, reorder, delay, caps, disconnects).
- Static analysis, sanitizers (ASan/UBSan/TSan where supported), strict
  warnings in CI.
- Every bug fix lands with a regression test.

The project is all C. Build and test with `cmd /c scripts\build_c.bat` on
Windows (Visual Studio / MSVC) or `sh scripts/build_c.sh` on Linux/macOS.
Harness commands and config files are listed in [HARNESS.md](HARNESS.md).

## What must never be done

See [AGENTS.md](AGENTS.md) and the hard rules in the standing contract:
no force-push/rewrite of `main`, no secrets or personal data, no unverified
parity/performance claims, no blocking OBS render/audio threads, no deleting
failed experiments, no touching files outside this repository (except a
dedicated test OBS profile), no publishing upstream-derived code before the
license check is logged.
