# exp_015 — Hypothesis: a friendlier Wormhole UX with sender identity and LAN discovery

**Date:** 2026-11-22
**Status:** hypothesis recorded before the verification runs; confirmed in FINDINGS.md.

## What we expect

1. The sender has a user-editable **IDENTIFIER** (name) exposed in its Tools-menu
   settings. The name is broadcast over UDP and carried in the v2 HELLO_ACK, so
   receivers know which machine they are linked to.
2. The receiver settings panel lists **auto-discovered Wormhole nodes** on the
   LAN (UDP 18092) and also accepts a **manual host/IP + TCP port**. Selecting a
   discovered node links to it automatically.
3. Both settings panels use a **cyberpunk / terminal aesthetic** with box-drawn
   banners, numbered sections, and a diagnostics button — while remaining
   pure C (OBS properties API only, no Qt/C++).
4. The existing video/audio path, Tools menu, and multi-audio behavior from
   exp_014 remain intact. No new crashes.

## What would prove us wrong

- Sender name absent from the wire or receiver log.
- Discovery finds nothing, or the receiver cannot link to a discovered node.
- Properties dialog crashes OBS; repeated stream descriptors flood the link.
- Video preview goes black or colored incorrectly.

## Method

- Extend v2 HELLO_ACK with a 128-byte sender name (backward-compatible parse:
  missing field becomes empty string). Update protocol tests.
- Add `plugin/src/discovery.c`: sender broadcasts `WORMHOLE2|name|port` to
  UDP 18092 once per second; receiver listens, keeps a 5-second peer table.
- Sender settings: IDENTIFIER field, `PRINT STATUS TO LOG` diagnostics button.
- Receiver settings: discovered-node combo box, `SCAN NETWORK` refresh button,
  manual HOST/PORT fields. Source thread resolves `peer` (ip:port) first, then
  falls back to manual host.
- Fix `need_desc` so a STREAM_DESC is emitted once per track, not per packet.
- Build with MSVC `/W4 /WX /DWIN32_LEAN_AND_MEAN` (resolves winsock.h/winsock2.h
  conflict introduced by including `<windows.h>` before `<winsock2.h>`).
- Re-run both portable OBS instances; verify via logs and screenshots.
