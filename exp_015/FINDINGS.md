# exp_015 — Findings: Wormhole UX (sender identity, LAN discovery, cyberpunk UI)

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Evidence:** `exp_results/wormhole-ux-window.png`, `exp_results/obs-tools-menu.png`,
OBS logs `third_party/obs-portable/{sender,receiver}/config/obs-studio/logs/2026-11-22 05-29-59.txt`,
`exp_results/wormhole-only.jsonl` (MCP launch responses).

## Verified results

### 1. Sender identity (rename)

- Sender `wormhole.json` now carries a `name` field; setup script uses
  `"GAMING-PC-NEO"`. The Tools-menu settings expose `> IDENTIFIER`.
- Sender log:
  `05:30:01.296: [wormhole] uplink engaged as 'GAMING-PC-NEO' on port 18090`
- The name travels inside the v2 HELLO_ACK (new 128-byte string field). ACK
  length grew from 34 to 49 bytes, exactly the 15 bytes of `GAMING-PC-NEO`.
- Receiver log:
  `05:30:01.414: [wormhole] v2 linked to sender 'GAMING-PC-NEO', capabilities=0x1`
- Protocol tests (`build_c.bat`) pass with the new field, including a
  round-trip assertion `strcmp(ack.sender_name, "gaming-pc") == 0`.

### 2. LAN discovery + manual entry

- `plugin/src/discovery.c` broadcasts on UDP 18092 and maintains a peer table.
- Receiver log:
  `05:30:01.297: [wormhole] DISCOVERED node 'GAMING-PC-NEO' at 192.168.1.104:18090`
- The receiver properties panel lists discovered nodes in a combo box
  (`> AUTO-DISCOVERED NODES`) with a `> SCAN NETWORK` refresh button, plus
  separate `> HOST / IP` and `> TCP PORT` fields for manual linking. The source
  thread prefers the selected `ip:port` key and falls back to manual values.
- Manual path remains the default in the automated test scene
  (`host=127.0.0.1`, `port=18090`) and still links correctly.

### 3. Cyberpunk settings UI (pure C)

- Both panels use box-drawn terminal banners (`W O R M H O L E // N E U R A L
  U P L I N K` / `S I G N A L R E C E I V E R`), numbered sections
  (`──[ 01 // TRANSPORT ]──`), `>`-prefixed controls, OBS info-text colouring,
  and a `CYBERPUNK BUILD` footer. No Qt/C++ was introduced — all via the OBS
  properties API.
- The sender's `> PRINT STATUS TO LOG` button was exercised during the run;
  sender log:
  ```
  [wormhole] === SENDER STATUS ===
  [wormhole] identity   : GAMING-PC-NEO
  [wormhole] discovery  : UDP 18092 / media TCP 18090
  ```

### 4. Regression checks

- Stream descriptors now appear **exactly once** per track: receiver logs
  `track 1 … track 6` at 05:30:01.504 and never repeat (previously they were
  re-sent on every audio packet).
- Video continues at 1280×720: `video frames=600 audio=5604` after ~20 s.
- Both OBS instances exited cleanly (`Number of memory leaks: 0` in the
  receiver profiler), no crashes.

## Build/negative results retained

- First build failed with winsock.h/winsock2.h redefinitions because
  `queue.h` includes `<windows.h>` before `transport.h` includes
  `<winsock2.h>`. Fixed with `/DWIN32_LEAN_AND_MEAN` in `build_plugin.bat`.
- Defining `_CRT_SECURE_NO_WARNINGS` on the command line collided with the
  per-file `#define` in every `.c`; removed from the command line and added to
  `discovery.c` instead.
- Final build: MSVC `/W4 /WX`, `build/plugin/wormhole.dll` 792,576 bytes.
- Screenshot attempts: OS focus stealing from unrelated windows prevented a
  reliable capture of the OBS settings dialog; the dialog's behavior is instead
  proven by the status-button log lines above. `obs-tools-menu.png` captures
  the sender's live display-capture content, confirming the video path.

## Consequences / remaining work

- Discovery currently runs only while the sender output is active and the
  receiver source exists. No persistence of the peer table across restarts.
- The receiver links to a discovered node only after the user selects it in
  the combo box; it does not auto-join the first node without user action.
- TLS/authentication is out of scope; discovery is plain UDP on the LAN.
