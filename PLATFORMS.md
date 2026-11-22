# PLATFORMS — Wormhole per-platform notes

Living document. Anything not verified on real hardware is explicitly marked
and mirrored in [RISKS.md](RISKS.md).

## Common requirements

- libobs headers and OBS Studio SDK for the installed OBS version.
- Rust toolchain (stable) and CMake; C compiler for the thin libobs layer.
- Typed TOML config; only `--config` / `--help` flags in tools.
- Core code stays portable; platform code behind small interfaces.

## Windows (primary development platform)

| Item | Value / status |
|---|---|
| Verified OS | Windows 11 Pro, build 26300 |
| OBS version | Studio 33.0.0, installed at `C:\Program Files\obs-studio` |
| OBS plugin dir (typical) | `%APPDATA%\obs-studio\plugins` and `C:\Program Files\obs-studio\obs-plugins\64bit` |
| Test profile | Dedicated OBS profile only; never the user's normal profile |
| Toolchain | Rust 1.99.0, CMake 4.3.3; Go **not installed** (needed for upstream recon) |
| GPU | NVIDIA GTX 1660 SUPER |
| Capture backend | To decide in Phase 2 (raw socket proxy vs Npcap) |
| Installer | Planned: Windows installer, CI-built with checksums |
| Signing | Not yet decided / flagged in RISKS.md |

Build and install steps for upstream Teleport: **to be filled in Phase 1** in
[HARNESS.md](HARNESS.md).

## Linux

| Item | Status |
|---|---|
| Build | CI on `ubuntu-latest` (planned) |
| Packaging | Planned: `.deb`/tarball; exact format to decide |
| Audio APIs | PipeWire, PulseAudio, ALSA considerations to document after port work |
| Video | libobs path; X11/Wayland capture quirks to investigate |
| Hardware verification | Not available locally; CI only unless a test machine is provided |

Open: which libobs/Qt packages the CI image provides; whether headless OBS
works in the runner for harness tests.

## macOS

| Item | Status |
|---|---|
| Build | CI on `macos-latest` and Apple Silicon runner if available (planned) |
| Local verification | **Not possible from this environment** — all macOS work relies on GitHub Actions runners |
| Code signing | Required for distribution; **not verified** — flagged in RISKS.md |
| Notarization | Steps to be documented here when implemented; **not verified** |
| Bundle format | Planned signed/notarized `.bundle`/`.pkg`; decide with evidence |
| Architectures | Intel vs Apple Silicon; universal binary decision pending |

Explicit limitation: no macOS behavior can be claimed as verified on real
hardware from the dev machine. Any macOS claim in PROGRESS.md must cite a CI
artifact and say it came from a runner.

## Platform isolation rule

Per the contract, platform-specific audio/video code stays behind small
interfaces so the Rust core is portable by construction. Any new platform API
call must be introduced behind such an interface with a documented portability
note here.
