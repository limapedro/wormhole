# HARNESS — reverse engineering and test harness

The harness makes Teleport's behavior fully observable and reproducible
without a human in the loop. It is implemented **entirely in C**; the former
Rust workspace was removed after its functionality was ported and verified
(`exp_005`–`exp_010`). This document describes each component and how to run it.

## Layout

```
include/wormhole/protocol.h   wire-format types and functions
include/wormhole/harness.h    typed config and command declarations
core/c/protocol.c             bounded decode/encode of Teleport framing
core/c/harness.c              typed TOML-subset config; check/synth/dissect
core/c/proxy.c                TCP capture/impairment proxy (Winsock/POSIX)
core/c/quality.c              JPEG frame decoding and image-quality metrics
core/c/differential.c         capture comparison with JSON report
core/c/media.c                deterministic I420 video + multi-track WAV
core/c/reference.c            standalone Teleport-protocol reference peers
core/c/harness_main.c         CLI: only --config and --help are flags
tests/test_protocol.c         C unit tests
tests/capture_writer.c        deterministic capture fixture writer
vendor/stb/                   public-domain stb_image / stb_image_write
scripts/build_c.bat|sh        MSVC / POSIX builds and tests
scripts/setup_portable_obs.ps1  configure two portable OBS copies
scripts/run_two_obs.ps1         launch pair, sample CPU, capture stream
harness/config/*.toml         typed configuration files
exp_001..exp_010/             hypotheses and findings (evidence)
```

Tools take a config path, not dozens of flags: `--config path/to.toml` plus
`--help`. Subcommands are positional.

## Components and status

| Component | Status | Purpose / evidence |
|---|---|---|
| Wire-format library | **Implemented** | Strict bounded decode/encode of 16-byte `Header`, 88-byte `ImageHeader`, 16-byte `WaveHeader`; unknown types preserved; unit tests |
| Capture/impairment proxy | **Implemented** | Records both directions (`capture` = client→target, `capture_down` = target→client); delay, deterministic drops, approximate bandwidth cap; propagates close. **pcap export and high-resolution per-byte timestamps are not yet implemented** |
| Protocol dissector | **Implemented** | Decodes capture to JSON and asserts byte-identical re-encode (`exp_007`) |
| Synthetic media generator | **Implemented** | I420 colour bars + moving marker across 720p30/1080p30/1080p60/1440p60/4K30, four S16LE WAV tracks (`exp_010`) |
| Standalone reference peers | **Implemented** | `ref-sender` / `ref-receiver` emit and decode real JPEG + PCM (`exp_010`) |
| Image-quality analysis | **Implemented** | Decodes captured JPEG frames; mean RGB, MSE, PSNR (`exp_009`) |
| Differential tester | **Stage 1 implemented** | Compares captures by type, timestamp and payload hash (`exp_004`, `exp_009`) |
| Two-portable-OBS monitor | **Implemented** | Scripts launch two independent portable OBS copies, sample CPU/memory, capture and score the stream (`exp_006`, `exp_008`) |
| Instrumented upstream build | Blocked | Go/mingw/libobs SDK unavailable; see build blockers below |
| Mock OBS host | Planned | Headless libobs host for CI; portable OBS pair used instead |
| Full S1–S10 benchmark runner | Planned | Scenario runner writing results into `exp_NNN/` |

## Build and commands

```bat
cmd /c scripts\build_c.bat
build\c\wormhole-harness.exe --help
build\c\wormhole-harness.exe --config harness/config/example.toml check
build\c\wormhole-harness.exe --config harness/config/example.toml synth
build\c\wormhole-harness.exe --config harness/config/example.toml dissect
build\c\wormhole-harness.exe --config harness/config/example.toml media
build\c\wormhole-harness.exe --config harness/config/obs-proxy.toml proxy
build\c\wormhole-harness.exe --config harness/config/obs-quality.toml quality
build\c\wormhole-harness.exe --config harness/config/c-reference.toml ref-sender
build\c\wormhole-harness.exe --config harness/config/c-reference.toml ref-receiver
```

On Windows the script locates Visual Studio with `vswhere`, runs
`vcvars64.bat`, and compiles with `cl /W4 /WX` (link `ws2_32.lib`). On
Linux/macOS, `sh scripts/build_c.sh` uses `cc -std=c11 -Wall -Wextra -Werror`
and links `libm`. CI runs both paths on all three platforms.

### Two portable OBS instances

```bat
powershell -File scripts\setup_portable_obs.ps1
powershell -File scripts\run_two_obs.ps1 -WindowSeconds 6 -Harness build\c\wormhole-harness.exe
build\c\wormhole-harness.exe --config harness/config/obs-proxy.toml dissect
build\c\wormhole-harness.exe --config harness/config/obs-quality.toml quality
```

The setup script creates two self-contained portable OBS copies under the
git-ignored `third_party/obs-portable/`, installs Teleport 0.8.0 into each,
replaces the bundled AJA/DeckLink/multi-source-record/obs-shaderfilter modules
that abort a clean OBS 33 launch with a no-op ABI stub compiled from
`scripts/obs_plugin_stub.c`, and enables OBS WebSocket v5 via
`plugin_config/obs-websocket/config.json` (ports 4455 and 4456). Verified: a
Win32 window-title check finds zero `Plugin Errors` dialogs after setup, and
both instances accept OBS WebSocket v5 requests. The run script samples
per-process CPU and working set, captures the stream through the proxy, and
writes `exp_results/two-obs-cpu.csv`.

The 1440p gaming PC → 1080p streaming PC scenario uses
`harness/config/obs-proxy-40m.toml` (40 Mbit/s cap) and the OBS-MCP workflow
described in `limapedro/OBS-MCP/docs/WORKFLOW.md`; results are in
[exp_011](exp_011/FINDINGS.md). The real 60-second NVENC H.264 recording
acceptance test on the streaming PC is in [exp_012](exp_012/FINDINGS.md).

Raw captures over a few MB are **not committed**; they are regenerated by
scripts. Only structured results and findings are committed.

## Building the Wormhole plugin (Windows)

The plugin is all C and builds against the exact OBS 33.0.0 headers. The
required SDK artifacts live in git-ignored `third_party/`:

```
powershell -File scripts/make_obs_import_lib.ps1
powershell -File scripts/make_obs_frontend_lib.ps1
cmd /c scripts\build_plugin.bat
```

The first two scripts extract the OBS source headers, create the empty
`obsconfig.h`, and generate `obs.lib` and `obs-frontend-api.lib` from the
installed DLL export tables. libjpeg-turbo 3.2.0 is built with NASM and
`WITH_CRT_DLL=TRUE` so it matches the plugin's `/MD` runtime. The build compiles
`plugin/src/*.c` plus the shared protocol core with MSVC `/W4 /WX /wd4201`
(the only suppressed warning is a nameless-union warning in libobs headers)
into `build/plugin/wormhole.dll`.

The plugin registers `wormhole-source`, `wormhole-output` and a Tools menu
item (Teleport-style). It uses SIMD JPEG, v2 negotiation, dynamic audio track
descriptors from OBS's own mixers, and a bounded queue so OBS threads never
block on network I/O. The sender has an editable `IDENTIFIER` broadcast on
UDP 18092 and carried in HELLO_ACK; the receiver discovers nodes on that port
or links manually by IP/port. Both settings panels use a cyberpunk/terminal
style built only with the OBS properties API (no Qt/C++).
`scripts/setup_portable_obs.ps1` configures the two portable OBS instances with
Teleport removed. See [exp_013](exp_013/FINDINGS.md),
[exp_014](exp_014/FINDINGS.md), [exp_015](exp_015/FINDINGS.md) and
[exp_016](exp_016/FINDINGS.md), [exp_017](exp_017/FINDINGS.md) and
[exp_018](exp_018/FINDINGS.md). The v0.2 scene mirror is verified with the
`verify-local-preserve2.ps1` and `verify-control.ps1` OBS-MCP scripts; results
are in `exp_results/`. Disabling the uplink is crash-free and calls
`obs_output_end_data_capture()` so OBS releases its raw-video lock and Output
Mode becomes editable again.

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
(`.github/workflows/build.yaml:7-18`). Reproducing that locally requires a Go
toolchain, a mingw C compiler, libobs headers and a SIMD libjpeg-turbo build;
none were available at recon time. Installing a system-wide toolchain is
outside the allowed workspace scope (see [AGENTS.md](AGENTS.md) hard rules),
so the source build remains blocked and is tracked as issue #6 / RISKS.md R13.

### Installer incident (honest record)

Running the release NSIS installer with
`/S "/D=E:\__PROJECTS__\Wormhole\third_party\upstream-bin\installed"` **did not
honor `/D`**: the upstream `.nsi` hardcodes `$INSTDIR` to
`$COMMONPROGRAMDATA\obs-studio\plugins\obs-teleport` in `.onInit`
(`build.yaml:107-109`). It therefore wrote `obs-teleport.dll` and
`uninstall.exe` to the pre-existing global plugin directory. No user OBS
profile or other project was modified; the install was left intact because a
Teleport installation predated this work.

### Fallback used for recon

The release DLL is used as the reference sender/receiver for protocol
verification. This gives a real, unmodified Teleport 0.8.0 binary without a
source build; it does **not** provide the tracing hooks of the planned
instrumented build (Phase 2, issue #15).

## Mock OBS host limits

Status: **planned**. In the meantime the two-portable-OBS monitor provides
real OBS 33.0.0 behavior on Windows. Anything that cannot be tested that way
(e.g. headless CI on Linux/macOS) will be documented here explicitly.

## Evidence conventions

- Every experiment has `exp_NNN/HYPOTHESIS.md` before it runs and
  `exp_NNN/FINDINGS.md` after, including negative results.
- Machine-readable results (`*.csv`, `*.json`) are git-ignored by default;
  promote small, meaningful summaries into `FINDINGS.md` text.
- A dissector change requires its round-trip test to pass before merge.

## Open questions

- Which capture backend is lowest-overhead (raw sockets vs pcap/Npcap)?
- Can latency be measured meaningfully in CI, or only on the dev machine?
- Does upstream expose enough of libobs to run under a lightweight mock?
