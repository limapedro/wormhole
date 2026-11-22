# HARNESS — reverse engineering and test harness

The harness makes Teleport's behavior fully observable and reproducible
without a human in the loop. It lives in `harness/` once code lands (Phase 2).
This document describes each component, how to run it, and what is currently
known. Planned commands are marked **planned** until implemented.

## Layout

```
Cargo.toml              workspace root
crates/protocol/        wh-protocol: byte-accurate Teleport framing (Rust library)
harness/                wormhole-harness CLI
  src/config.rs         typed TOML config structs with explicit defaults
  src/dissect.rs        capture -> structured messages, round-trip validation
  src/proxy.rs          transparent TCP capture/impairment proxy
  src/media.rs          deterministic I420 video + S16LE audio generators
  src/synth.rs          deterministic Teleport-compatible capture fixture
  src/replay.rs         replay capture to proxy; minimal test sink
  src/bench.rs          dissector throughput self-benchmark (not Teleport)
  config/               example.toml, proxy-test.toml, proxy-dissect.toml
exp_001/                first fidelity experiment (HYPOTHESIS + FINDINGS)
```

Planned for later phases: `instrumented/` (patched upstream), `mock_obs/`
(headless libobs host), `reference/` (standalone peers), `differential/`
(Teleport vs Wormhole comparison), and the full scenario benchmark runner.

Tools take a config path, not dozens of flags: `tool --config path/to.toml`
(plus `--help`). Only `--config` and `--help` are accepted flags; subcommands
are positional.

## Components and status

| Component | Status | Purpose |
|---|---|---|
| Wire-format library (`wh-protocol`) | **Implemented** | Strict bounded decode/encode of the 16-byte header, 88-byte `ImageHeader` and 16-byte `WaveHeader`; unknown types preserved; 10 unit tests |
| Capture/impairment proxy | **Implemented (basic)** | Records sender→receiver bytes to a raw capture; injects fixed delay, deterministic drop percentage and an approximate bandwidth cap. **pcap export and high-resolution per-byte timestamps are not yet implemented** |
| Protocol dissector | **Implemented** | Decodes a capture to JSON summaries and asserts byte-identical re-encode |
| Synthetic media generator | **Implemented (basic)** | Deterministic I420 color bars with moving marker; S16LE WAV with a 440/880 Hz sync tone. Multiple resolutions and multi-track are not yet implemented |
| Capture fixture, replay and sink | **Implemented** | Produce and replay deterministic Teleport-compatible byte streams for unattended tests |
| Dissector self-benchmark | **Implemented** | Measures harness encode/decode throughput only — explicitly **not** a Teleport or Wormhole benchmark |
| Instrumented upstream build | Blocked | Patched Teleport with tracing: frame timings, allocations, GC pauses, cgo calls, queue depths; kept separate from Wormhole code. See build blockers below |
| Mock OBS host | Planned | Load/exercise a plugin without full OBS GUI in CI; fallback: headless/scripted OBS, limits documented |
| Reference sender/receiver | **Implemented** | Standalone Rust peers (`ref-sender`, `ref-receiver`) that emit/decode real JPEG + PCM in the documented wire format; validated in [exp_002](exp_002/FINDINGS.md) |
| Differential tester | Planned | Compare decoded output, timing, bandwidth and failure behavior |
| Full benchmark runner | Planned | Run `PERFORMANCE_TARGETS.md` scenarios S1–S10; write CSV/JSON + summary into `exp_NNN/` |

## Running (implemented commands)

```bash
cargo test --workspace --all-targets
cargo fmt --all -- --check
cargo clippy --workspace --all-targets -- -D warnings

# Print the effective typed configuration
cargo run -p wormhole-harness -- --config harness/config/example.toml check

# Deterministic capture fixture -> dissector (round-trip assertion)
cargo run -p wormhole-harness -- --config harness/config/example.toml synth
cargo run -p wormhole-harness -- --config harness/config/example.toml dissect

# Synthetic media
cargo run -p wormhole-harness -- --config harness/config/example.toml media

# End-to-end proxy test: sink + proxy in separate terminals, then:
cargo run -p wormhole-harness -- --config harness/config/proxy-test.toml sink
cargo run -p wormhole-harness -- --config harness/config/proxy-test.toml proxy
cargo run -p wormhole-harness -- --config harness/config/proxy-test.toml replay
cargo run -p wormhole-harness -- --config harness/config/proxy-dissect.toml dissect

# Standalone reference peers through the proxy
cargo run -p wormhole-harness -- --config harness/config/reference-test.toml ref-receiver
cargo run -p wormhole-harness -- --config harness/config/proxy-test.toml proxy
cargo run -p wormhole-harness -- --config harness/config/reference-test.toml ref-sender
cargo run -p wormhole-harness -- --config harness/config/reference-test.toml dissect

# Harness self-benchmark (not a product benchmark)
cargo run -p wormhole-harness -- --config harness/config/example.toml bench
```

Fidelity evidence for these commands is in [exp_001](exp_001/FINDINGS.md).

Raw captures over a few MB are **not committed**; they are regenerated by
scripts or attached to releases. Only structured results and findings are
committed.

## Building original Teleport on Windows (Phase 1 attempt — blocked)

### What was done (2026-11-22)

1. Cloned `https://github.com/fzwoch/obs-teleport.git` into git-ignored
   `third_party/obs-teleport/`; HEAD `e8c49f4be390ed17c434099541ce2fd65720e4c8`,
   release tag `0.8.0` = `b61ee3c63f629bca75e7571bb7bdd151d8ee5aa6`.
   `git diff 0.8.0 HEAD` changes only `.github/workflows/build.yaml`.
2. Cloned dependency `schollz/peerdiscovery` v1.7.6
   (`e0ee252378e1b36c6e9a70727043141556fc0d99`) into git-ignored
   `third_party/peerdiscovery/` to read discovery behavior.
3. Read all upstream source (~2,400 lines) and recorded the protocol in
   [PROTOCOL.md](PROTOCOL.md).
4. Downloaded the official `0.8.0` Windows x64 release into git-ignored
   `third_party/upstream-bin/`; extracted DLL SHA-256
   `7BC1412CB289B36B3DB6736E48F1AA8CA1D06DB8F04ABEB25EFA81456A1E45C6`.

### Build from source: blocked (negative result, recorded deliberately)

Upstream does **not** build natively on Windows; its CI uses the
`fzwoch/klumpatsch` Docker image on `ubuntu-latest` with a mingw cross-compiler
(`.github/workflows/build.yaml:7-18`). Reproducing that locally requires:

| Requirement | Local state | Evidence |
|---|---|---|
| Go toolchain | **not installed** (`go` not on PATH) | `Get-Command go` failed, 2026-11-22 |
| C compiler (mingw/clang/MSVC) | **none installed** — `clang`, `gcc`, `cl`, `x86_64-w64-mingw32-gcc` all absent | `Get-Command` probe, 2026-11-22 |
| libobs headers + `obs.dll`/`obs-frontend-api.dll` | OBS Studio 33.0.0 installed, but no SDK headers in the install tree | `Get-ChildItem 'C:\Program Files\obs-studio' -Recurse -Filter *.h` returned nothing |
| libjpeg-turbo 3.2.0 built with SIMD | not present | — |

Installing a system-wide toolchain is outside the allowed workspace scope
(see [AGENTS.md](AGENTS.md) hard rules), so the source build remains blocked
and is tracked as issue #6 / RISKS.md R13. The upstream CI recipe is the
reference if a self-contained portable toolchain is later placed under
git-ignored `third_party/`.

### Upstream CI build recipe (reference, not executed locally)

```sh
# from .github/workflows/build.yaml, Windows x86_64 job
cmake -B /tmp/turbojpeg -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DREQUIRE_SIMD=TRUE -DENABLE_SHARED=FALSE -DWITH_TOOLS=FALSE \
  -DWITH_TESTS=FALSE -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
  -DCMAKE_TOOLCHAIN_FILE=/cmake/mingw-x86_64.cmake /tmp/libjpeg
cmake --build /tmp/turbojpeg
# OBS 31.1.0 headers + DLLs unpacked into /tmp/obs
CGO_ENABLED=1 GOOS=windows GOARCH=amd64 CC=x86_64-w64-mingw32-clang \
  CGO_CFLAGS="-O2 -I/tmp/obs/libobs -I/tmp/obs/frontend/api -I/tmp/libjpeg/src -I/tmp/turbojpeg -I/tmp" \
  CGO_LDFLAGS="-L/tmp/turbojpeg -lturbojpeg -L/tmp/obs -lobs -lobs-frontend-api" \
  go build -buildvcs=false -ldflags="-X main.version=0.8.0" \
  -buildmode=c-shared -o obs-teleport.dll
```

### Installer incident (honest record)

Running the release NSIS installer with
`/S "/D=E:\__PROJECTS__\Wormhole\third_party\upstream-bin\installed"` **did not
honor `/D`**: the upstream `.nsi` hardcodes `$INSTDIR` to
`$COMMONPROGRAMDATA\obs-studio\plugins\obs-teleport` in `.onInit`
(`build.yaml:107-109`). It therefore wrote `obs-teleport.dll` and
`uninstall.exe` to the pre-existing global plugin directory
(`C:\ProgramData\obs-studio\plugins\obs-teleport`, created 2026-08-19). The
DLL hash matches the 0.8.0 release. No user OBS profile or other project was
modified. The install was left in place rather than uninstalled, because a
Teleport installation predated this work and removing it would destroy the
user's existing setup. A dedicated test profile must be configured before any
behavioral experiments.

### Fallback used for recon

The release DLL is used as the reference sender/receiver for protocol
verification. This gives a real, unmodified Teleport 0.8.0 binary without a
source build; it does **not** provide the tracing hooks of the planned
instrumented build (Phase 2, issue #15).

## Mock OBS host limits

Status: **unknown — to be determined in Phase 2.** If a full libobs mock is
not feasible, headless/scripted OBS will be used and the untestable areas
(e.g. GUI settings behavior) documented here explicitly.

## Evidence conventions

- Every experiment has `exp_NNN/HYPOTHESIS.md` before it runs and
  `exp_NNN/FINDINGS.md` after, including negative results.
- Machine-readable results (`*.csv`, `*.json`) are git-ignored by default;
  promote small, meaningful summaries into `FINDINGS.md` text.
- A dissector change requires its round-trip test to pass before merge.

## Open questions

- Which capture backend on Windows is lowest-overhead (raw socket proxy vs
  pcap/Npcap)? Decide with an experiment.
- Can latency be measured meaningfully in CI, or only on the dev machine?
- Does upstream expose enough of libobs to run under a lightweight mock?
