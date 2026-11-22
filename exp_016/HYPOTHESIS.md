# exp_016 — Hypothesis: disabling the Wormhole output must be crash-free and must unlock OBS output settings

**Date:** 2026-11-22
**Status:** hypothesis recorded before the verification runs; confirmed in FINDINGS.md.

## Symptom reported by the user

- Unchecking `ENGAGE UPLINK` on the gaming PC (sender) crashed OBS.
- After disabling Wormhole, OBS Settings → Output Mode stayed greyed out on
  the gaming PC while it was editable on the streaming PC (receiver).

## What we expect

1. Disabling the output stops it exactly once — no double stop callback,
   no double thread join or socket close.
2. OBS stays alive through repeated disable/re-enable cycles.
3. The receiver reconnects automatically when the sender comes back.
4. Once Wormhole is disabled, `obs_video_active()` becomes 0 and OBS allows
   switching Output Mode from Simple to Advanced.

## What would prove us wrong

- Two `output stop requested` lines (the pre-fix crash signature).
- OBS process disappears after a disable.
- `obs_video_active()` stays 1 after disable, or Output Mode cannot change.

## Method

- Read libobs 33.0.0 source to trace the output lifecycle.
- Add idempotent stop guard, move `obs_output_begin_data_capture()` onto
  OBS's thread, stop double-closing sockets, and call
  `obs_output_end_data_capture()` from the output stop callback.
- Add `shutdown()` before `close()` so blocking sends are interrupted.
- Add an automatic reconnect loop to the receiver source.
- Expose `obs_video_active()` in the sender diagnostics button for evidence.
- Verify via OBS WebSocket disable/re-enable cycles and OBS logs.
