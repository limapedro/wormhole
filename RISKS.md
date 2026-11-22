# RISKS and open questions

Each risk has an owner action. When resolved, replace the description with a
short resolution and a link to the evidence/decision; do not delete the row.

| # | Risk / question | Impact | Owner action | Status |
|---|---|---|---|---|
| R01 | License compatibility: upstream obs-teleport license and GPL obligations from linking libobs are unverified | Could block publishing any derived code; wrong license = legal problem | — | **Resolved 2026-11-22:** upstream is GPL-2.0-or-later, libobs is GPL-2.0-or-later; Wormhole licensed GPL-2.0-or-later, logged in ARCHITECTURE_LOG.md and LICENSE finalized. No upstream code copied. |
| R02 | Go toolchain not installed on dev machine | Cannot build upstream Teleport from source for recon/baseline/instrumentation | Use portable Go + C toolchain under git-ignored `third_party/` (system install is outside allowed scope), or keep using the official 0.8.0 release DLL for behavioral tests | Open (mitigated for recon by release DLL) |
| R03 | macOS cannot be tested on real hardware locally | Platform-specific bugs and notarization issues may go unnoticed | Use GitHub Actions macOS + Apple Silicon runners; mark all macOS claims as CI-only; document notarization steps in PLATFORMS.md | Open |
| R04 | OBS API changes across versions (33.0.0 locally, older on CI/other platforms) | Plugin may break on user systems | Pin target OBS SDK, test against installed 33.0.0 and CI images; isolate libobs calls behind thin C layer | Open |
| R05 | Performance targets are hypotheses, not measured | Could chase impossible targets or accept regressions | Complete `exp_000` baseline early; revise targets with evidence in PERFORMANCE_TARGETS.md | Open |
| R06 | Full mock OBS host may be infeasible | Headless CI testing limited | Evaluate in Phase 2; if infeasible use scripted/headless OBS and document exact limits in HARNESS.md | Open |
| R07 | mDNS/UDP discovery behavior on each OS differs | Discovery parity may fail on Linux/macOS | Capture discovery per platform with the proxy; document TXT/service layout in PROTOCOL.md | Open |
| R08 | **Resolved 2026-11-22 by exp_018:** output-track model chosen (`raw_audio2` + `obs_output_set_mixers`); per-source capture retained only as history in exp_017. |
| R09 | **Resolved 2026-11-22 by exp_018:** receiver presentation is `wormhole-audio` clones named after sender sources, in additive `WH ` scenes that preserve local mic/webcam. |
| R08-old | Multiple audio outputs mapping to OBS is undecided | Wrong design could require rework | Experiment with libobs per-track audio exposure (6 mixer tracks vs sources vs both); log decision in ARCHITECTURE_LOG.md | Open |
| R09 | Receiver-side presentation of multiple tracks is undecided (OBS sources emit one audio stream) | UX and routing issues on receive side | Evaluate companion sources vs mixer routing with evidence; log decision | Open |
| R10 | Large captures / binaries in repo | Bloats history, leaks environment details | Keep raw captures git-ignored; regenerate via scripts or attach to releases; commit only summaries | Mitigated by .gitignore |
| R11 | Windows driver/signing for installer | Distribution friction | Decide signing approach before release; document in PLATFORMS.md | Open |
| R12 | Upstream source availability/commit stability | Recon could target a moving branch | Clone once into git-ignored third_party; pin exact commit hash in PROTOCOL.md | Mitigated: HEAD `e8c49f4…`, tag 0.8.0 `b61ee3c…` recorded |
| R13 | No C compiler or libobs SDK headers locally; upstream builds only via a Linux Docker/mingw cross-toolchain | Cannot produce an instrumented upstream build for Phase 2 tracing or exact source-build reproduction | Place a self-contained portable toolchain (Go + mingw/clang + headers + libjpeg-turbo) under git-ignored `third_party/`, or run the upstream CI recipe on GitHub Actions and use its artifacts; document outcome in HARNESS.md | **Open — blocks instrumented build (issue #6/#15)** |
| R14 | NSIS release installer ignores `/D` and wrote to the pre-existing global OBS plugin directory | Violates the "dedicated test profile only" rule if repeated | Do not run the installer again; use the already-extracted DLL from `third_party/upstream-bin/` and copy it into a dedicated test profile when needed; incident recorded in HARNESS.md | Mitigated: incident documented, no profile data touched |
| R15 | Wire audio `Format` field carries a libobs enum integer that changed across OBS versions | A compatibility-mode receiver on a different OBS version could misinterpret audio format | Capture from an older OBS sender (or read older headers) and define an explicit mapping in PROTOCOL.md / v2 spec | Open |

## Questions only the user can decide

Per the standing contract, stop and ask the user only for decisions outside the
repository or requiring money/personal approval. Known candidates:

- Final license choice if the license check leaves more than one valid option.
- Whether to publish any upstream-derived code if the license situation is
  ambiguous (hard rule: never before the check is logged).
- Purchasing code-signing certificates / notarization credentials.
