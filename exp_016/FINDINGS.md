# exp_016 — Findings: crash-free disable and unlocked output settings

**Date:** 2026-11-22
**Hypothesis:** [HYPOTHESIS.md](HYPOTHESIS.md)
**Evidence:** `exp_results/wormhole-active-flag2.jsonl`,
`exp_results/wormhole-mode-final.jsonl`,
OBS logs `third_party/obs-portable/{sender,receiver}/config/obs-studio/logs/2026-11-22 06-01-32.txt`.

## Root causes found

1. **Double stop → crash.** `wh_control_update()` called `obs_output_stop()`
   only when `obs_output_active()` was true. libobs clears that flag
   asynchronously, so `obs_output_release()` invoked our stop callback a
   second time. The second `wh_thread_join()` / `CloseHandle()` on an already
   closed thread handle crashed OBS. The pre-fix log ended with two
   `output stop requested` lines and no clean shutdown.
2. **Blocking shutdown hang.** `closesocket()` from the UI thread does not
   reliably interrupt an in-flight blocking `send()` on the worker thread;
   `wh_thread_join()` could hang forever (observed: stop logged, OBS froze).
3. **Output Mode greyed out forever.** libobs does **not** end raw data
   capture in the normal `obs_output_stop()` path — the output plugin must
   call `obs_output_end_data_capture()` itself. Without it, the canvas keeps
   `raw_active > 0`, so `obs_video_active()` stays true and OBS disables the
   Settings → Output Mode combo. The receiver had no output, hence the
   difference between the two instances.

## Fixes

- `wh_output_stop()` guarded by a `thread_valid` flag: exactly one caller
  performs teardown. Stop is now called unconditionally from the control
  source; `wh_control_shutdown()` is shared with source destroy.
- Worker thread uses a stable local socket copy; stop owns closing it.
  `obs_output_begin_data_capture()` moved from the worker thread into
  `wh_output_start()` so no OBS API is called from a foreign thread.
- `wh_net_close()` now calls `shutdown(sock, SD_BOTH)` before closing.
- `wh_output_stop()` calls `obs_output_end_data_capture()` after joining.
- Receiver source refactored into a reconnecting session loop; it resets its
  clock offset and retries every second while the source is alive.
- Queued frames are drained back to their pools on stop (no leaks).

## Verified results

- `STOP_COUNT=1` after a disable (previously 2). Repeated disable/re-enable
  cycles completed with both OBS processes alive.
- Sender diagnostics button reports the exact transition:
  ```
  06:02:32.464: [wormhole] obs_video_active=1 (0 means Output Mode is editable)
  06:02:34.259: [wormhole] data capture ended
  06:02:37.768: [wormhole] obs_video_active=0 (0 means Output Mode is editable)
  06:02:46.804: [wormhole] obs_video_active=1 (0 means Output Mode is editable)
  ```
- With Wormhole disabled, `SetProfileParameter Output/Mode=Advanced` succeeded:
  `GetProfileParameter` returned `parameterValue: "Advanced"` and
  `basic.ini` contains `Mode=Advanced`. OBS stayed alive (CPU ~1.5 %).
- After re-enable, receiver logged `link dropped … retrying`, then
  `DISCOVERED node 'GAMING-PC-NEO'` and `v2 linked to sender 'GAMING-PC-NEO'`
  with video frames continuing — automatic recovery works.
- `build_c.bat` and MSVC `/W4 /WX` plugin build pass.

## Consequences

Disabling Wormhole is now safe and releases OBS's output settings. The
`obs_video_active` diagnostic remains in the sender panel. A remaining design
limitation: while Wormhole is enabled, OBS will still grey out Output Mode —
that is libobs policy for any active raw video output, not a Wormhole bug; the
user must disable the uplink first (now crash-free).
