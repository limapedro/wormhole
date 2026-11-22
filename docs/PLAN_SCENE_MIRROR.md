# Plan: Wormhole scene mirror (one video signal, N audio channels, synced UI)

**Status:** plan only. No code, commits, branch switches, or edits to the
exploratory working tree are authorised by this document.

## 0. Working-tree report (required before anything else)

Commands run: `git status`, `git branch --show-current`, `git diff --stat`,
`git ls-files --others --exclude-standard`. Output at the time of planning:

- Branch: `main`, clean HEAD `a972bdb` (`fix: crash-free Wormhole disable and
  release OBS output settings (#78)`), up to date with `origin/main`.
- Modified (tracked): `core/c/protocol_v2.c`, `include/wormhole/protocol_v2.h`,
  `plugin/src/output.c`, `plugin/src/plugin.c`, `plugin/src/source.c`.
- Untracked: `docs/PLANNING_PROMPT.md`, `exp_017/HYPOTHESIS.md`
  (`exp_017/` contains only that file — no `FINDINGS.md`).
- Diff stat: 5 files, **+530 / -72**.
- No files were stashed, reset, restored, or cleaned.

All of the above is treated as **exploratory evidence**, not a baseline.

## 1. Uncommitted work triage (per file)

| File | What the edit attempts | Conflict with accepted behaviour / evidence | Proposed disposition |
|---|---|---|---|
| `core/c/protocol_v2.c` (+21) | Adds `wh2_build_text` / `wh2_parse_text` using existing `begin_frame`/`put_string`/`get_string`. | No conflict; generic text frames are compatible with v2 framing. **Bug:** file has no trailing newline; `wh2_parse_text` does not validate that the text fits inside `header.total_len` beyond what `get_string` does (it passes `header.total_len`, which is correct). | **Keep as starting point** for the protocol branch, but add parser unit tests in `tests/test_protocol_v2.c` (none exist for the new functions) and fix EOF newline. |
| `include/wormhole/protocol_v2.h` (+9/-...) | Adds `WH2_MSG_SCENE_REQUEST=20`, `WH2_MSG_SCENE_MANIFEST=21` and the two text helpers. | No conflict, but `SCENE_REQUEST` is **declared and never sent or handled**; the sender pushes the manifest unconditionally after negotiation. `PROTOCOL.md` Part 2 is still a draft and documents neither message. | **Keep**, then implement the request path and update `PROTOCOL.md` in the same PR. |
| `plugin/src/plugin.c` (+2) | Registers a new `wormhole-audio` source type. | No conflict. | **Keep.** |
| `plugin/src/output.c` (+311/-...) | (a) Replaces `raw_audio2` mixer tracks with per-source pre-mix capture via `obs_source_add_audio_capture_callback`; (b) adds `WH_MAX_AUDIO_SOURCES=16`, bindings with strong refs, cleanup on stop/destroy; (c) adds a line-based scene manifest builder and sends it after negotiation. | (a) **Directly conflicts with the user's new requirement** to match the gaming PC's *output* audio tracks; it also failed to deliver two of five bound sources in testing (OBS monitoring dedup). It removes `obs_output_set_mixers` and `OBS_OUTPUT_MULTI_TRACK`, so track-1 Teleport fallback semantics from `PROTOCOL.md` are lost. (b) Cleanup is correct in principle. (c) **Verified broken**: `wh_manifest_append` writes `prefix + text + suffix` but never writes the `':'` separator, so the wire contains `SScene\nIMain Monitorvideo\n...` (76 bytes) and the receiver's `S:` / `I:` parser matches zero lines. The proactive push also blocks the worker thread on `obs_queue_task(..., wait=true)`. | **Rewrite** the audio half (restore track mode; keep per-source only as opt-in companion). **Rewrite** the manifest as a structured control message. **Keep** the binding/cleanup pattern as reference for control-listener cleanup. |
| `plugin/src/source.c` (+259/-...) | Adds `wormhole-audio` source type, clone creation/cleanup tasks, manifest application task, `SYNC SCENES` button, routes audio by `track_id` to clones instead of main source, removes `OBS_SOURCE_AUDIO` from the main source. | Clone creation and refcounts are reasonable, but: cleanup only scans `obs_frontend_get_current_scene()` so clones left in other scenes leak; `child_requested[]` is never reset on reconnect, so re-sync cannot rebuild; the manifest parser is dead code because of the missing colons; the `SYNC SCENES` button reconnects, which is a workaround for the absent request path; removing `OBS_SOURCE_AUDIO` from `wormhole-source` is correct only if all audio moves to clones. | **Rewrite** as part of the control/mirror branch; **keep** the clone refcount pattern and the `obs_queue_task(OBS_TASK_UI, ...)` threading pattern. |
| `docs/PLANNING_PROMPT.md` (untracked) | The prompt I produced in the previous turn. | Superseded by this plan. | **Discard or keep as historical note** — owner's choice; it is not a design input. |
| `exp_017/HYPOTHESIS.md` (untracked) | Records the per-source-audio hypothesis. | Describes the now-superseded source-based model, but is valid evidence of what was tried. `exp_017/FINDINGS.md` is missing. | **Keep and complete** with a FINDINGS.md that records the verified results (5 bound sources, 3 callbacks, 3 clones) and the manifest-colon bug — negative results are not deleted per `AGENTS.md`. |

**Disagreements requiring the owner's decision (do not silently pick one):**

1. `GOAL.md` priority order puts Teleport parity and measured efficiency before multi-audio; `ROADMAP.md` Phase 6 says the audio mapping decision must be evidenced and logged. The exploratory work implements per-source audio, while the user's latest instruction asks for **output-track** channels. Section 3 recommends track mode as authoritative and source mode as opt-in; please confirm before implementation.
2. `PROTOCOL.md` Part 2 is explicitly a draft and says v2 must fall back to Teleport track-1 WAVE. The uncommitted edits branch 1 to clones and drop `OBS_OUTPUT_MULTI_TRACK`. Recommendation: preserve the fallback in track mode; confirm.
3. `RISKS.md` R08/R09 are still open and literally cover these two decisions. This plan proposes resolving both with `exp_018`; confirm that is acceptable.

**Branch proposal (after approval only):** create `feat/scene-mirror` from `a972bdb`. Carry the exploratory edits onto it as a single **WIP commit** *or* reimplement from HEAD per triage — the owner chooses. No commit, amend, rebase, or branch switch happens while planning.

## 2. Verified evidence from the exploratory state (testing done during planning)

Commands: `scripts/build_c.bat` and `scripts/build_plugin.bat` both pass (MSVC `/W4 /WX`); both portable OBS instances launch and stream. From
`third_party/obs-portable/sender/.../2026-11-22 06-41-51.txt` and the receiver log:

```
[wormhole] bound audio source 1: 'Desktop Audio'
[wormhole] bound audio source 2: 'Mic/Aux'
[wormhole] bound audio source 3: 'Desktop Audio 2'
[wormhole] bound audio source 4: 'Mic One'
[wormhole] bound audio source 5: 'Mic Two'
[wormhole] 5 audio source(s) bound
[wormhole] audio cb 'Mic/Aux': frames=480 muted=0 layout=2 ...
[wormhole] audio cb 'Mic One': frames=480 muted=0 layout=2 ...
[wormhole] audio cb 'Mic Two': frames=480 muted=0 layout=2 ...
[wormhole] scene manifest built: 76 bytes
[wormhole] manifest:
SScene
IMain Monitorvideo
IDesktop Audio 2audio
IMic Oneaudio
IMic Twoaudio
```
Receiver: `received scene manifest (76 bytes)`, `scene manifest applied`, but
`GetSceneItemList` contained only `Wormhole` + the three descriptor-driven
clones — proving the manifest parser matched nothing because of the missing
`:` separators. `GetSceneList` on both sides reported only `Scene`, so no
scenes were created from the manifest.

Conclusion: per-source capture works for 3/5 sources; scene mirroring does
**not** work yet and must be treated as unimplemented, not as a broken feature
to patch in place.

## 3. Architecture

### 3.0 Non-destructive merge — owner's late constraint (supersedes 3.3 details)

The owner pointed out a scenario the earlier plan breaks: the streaming PC may
only want **gameplay from the gaming PC** while keeping its **own microphone and
webcam** as local sources. A wholesale scene-collection copy would destroy
those local sources. Therefore:

- The receiver's existing scenes and sources are **never** deleted, renamed or
  modified. No scene-item removal is performed on local sources.
- Before the first mirror, snapshot the receiver's current scene items whose
  source id does **not** start with `wormhole` (local mic, webcam, etc.) with
  their `obs_transform_info` and visibility.
- For each sender scene, create a **new** scene named `WH <sender scene>`
  (prefix always, so a local scene with the same name is never overwritten).
- Each mirrored scene receives: the single Wormhole video source, one
  `wormhole-audio` clone per track, and copies of the snapshotted local sources
  with their transforms (`obs_sceneitem_get_info2` / `set_info2`,
  `obs.h:1826–1827`).
- Control events: scene switch maps the sender's active scene to its
  `WH ` counterpart; mute/hide events are tracked by Wormhole clone identity
  and are never applied to local sources.
- Re-sync is idempotent: existing `WH ` scenes and clones are reused, never
  duplicated.

This changes the UI promise slightly: the streaming PC's Sources list contains
both its local devices and the Wormhole clones, which is exactly what the
owner's scenario requires.

### 3.1 One video signal (keep)

Keep the existing `obs_output_raw_video` → JPEG → `WH2_MSG_VIDEO` path and the
receiver's `obs_source_output_video2`. The user explicitly wants one
composited program feed, so **do not** add per-source video capture.

### 3.2 Audio network channels = sender output tracks

Recommendation (needs owner confirmation, see triage): make the authoritative
audio model the **sender's OBS mixer/output tracks**:

- Restore `OBS_OUTPUT_AV | OBS_OUTPUT_MULTI_TRACK` and `raw_audio2`, and
  `obs_output_set_mixers()` with a mask from settings (default tracks 1–2,
  range 1–6). `raw_audio2` fires for active mixer tracks regardless of source
  dedup, which fixes the observed `Desktop Audio 2` gap.
- `track_id = mixer_index + 1`; name from settings (`Track 1`, …) or, if
  readable from the profile config, OBS's own track names. Shared clock ID 1
  for all tracks, as v2 already specifies.
- Receiver creates one `wormhole-audio` clone per track **named exactly after
  the sender's track**, and routes `WH2_MSG_AUDIO` by `track_id`. This is what
  the streaming PC mixes/outputs.
- Keep the per-source pre-mix capture code behind an opt-in setting
  `source_audio=on` for companion clones named after the sender's *sources*;
  never enable both models on the same connection, to avoid duplicate audio.

### 3.3 Scene/control channel

Reuse the existing TCP media connection for control frames in **both**
directions; do not open another port:

- Sender: add a bounded non-blocking reader (`select()`/FIONREAD, zero timeout)
  to the send loop, polled at ~20–50 ms; read only control message types.
- Receiver: already has a read loop; add handlers for manifest and incremental
  events.
- `SCENE_REQUEST` (receiver → sender) triggers a fresh full snapshot. The
  Tools-menu **SYNC SCENES** button sends this instead of force-closing the
  socket.
- Sender pushes a full snapshot once after negotiation for zero-click sync.

Full snapshot payload (structured, not the current `S:`/`I:` text):

```
scene_count, active_scene
per scene: name, order, item_count
per item: source_name, kind, track_id (for audio), visible, enabled,
          x, y, scale_x, scale_y, rotation
per audio track: id, name, muted
```

Incremental events: `SCENE_SWITCH`, `ITEM_VISIBILITY`, `SOURCE_MUTE`,
`ITEM_ADDED`, `ITEM_REMOVED`. Sender is authoritative initially;
receiver→sender control is a later decision (see Risks).

All OBS mutations (scene create, source create, `obs_sceneitem_set_visible`,
`obs_source_set_muted`, `obs_frontend_set_current_scene`) run on the UI thread
via `obs_queue_task(OBS_TASK_UI, ...)` with a copied snapshot; the worker
thread only serializes/parses and queues.

## 4. Protocol extensions

- Add capability `WH2_CAP_SCENE_SYNC (1u << 3)` to HELLO/HELLO_ACK; only send
  control frames when both sides advertise it. Existing capability bits stay.
- Keep `WH2_MSG_SCENE_REQUEST=20` and `WH2_MSG_SCENE_MANIFEST=21`; change their
  payload from free text to the structured snapshot above (versioned with a
  `manifest_version` field so old peers can be rejected cleanly).
- Add `WH2_MSG_CONTROL=22` with `uint16 event_type`, `uint32 sequence`,
  `uint64 timestamp_ns`, then a small TLV body per event.
- Add `WH2_MSG_AUDIO_TRACKS=23` for the track-name/id table so the receiver can
  name clones before the first audio packet.
- Backward compatibility: v1/legacy peers receive track 1 as ordinary Teleport
  WAVE (already implemented); new messages are ignored if capability absent.
- Add unit tests in `tests/test_protocol_v2.c` for every new builder/parser,
  including truncated and oversized payloads.

## 5. OBS API research (verified against local OBS 33 SDK)

All paths are relative to `third_party/obs-sdk/obs-studio-33.0.0/`.

**Scene reading / mutation**

- `frontend/api/obs-frontend-api.h:124` — `EXPORT char **obs_frontend_get_scene_names(void);`
- `frontend/api/obs-frontend-api.h:125` — `EXPORT void obs_frontend_get_scenes(struct obs_frontend_source_list *sources);` with the inline `obs_frontend_source_list_free()` at `obs-frontend-api.h:80–86`.
- `frontend/api/obs-frontend-api.h:126–127` — `obs_frontend_get_current_scene()` / `obs_frontend_set_current_scene(obs_source_t*)`.
- `libobs/obs.h:1743` — `obs_scene_enum_items(obs_scene_t*, bool (*)(obs_scene_t*, obs_sceneitem_t*, void*), void*)`. The exploratory edit originally had the callback parameters in the wrong order; compiler warning `C4113` caught it and it was fixed — a good argument for keeping `/W4 /WX`.
- `libobs/obs.h:1793` — `obs_sceneitem_get_source`; `libobs/obs.h:1834` — `obs_sceneitem_set_visible(item, visible)`.
- `libobs/obs.h:1697` — `obs_scene_create(name)`; `obs.h:1713` — `obs_scene_release`; `obs.h:1718` — `obs_scene_get_source`; `obs.h:1760` — `obs_scene_add`; `obs.h:1724` — `obs_scene_find_source`; `obs.h:1769` — `obs_sceneitem_remove`.
- `libobs/obs.h:1091` — `obs_source_get_ref`; `obs.h:1201` — `obs_source_set_name`.

**Mute state**

- `libobs/obs.h:1332` — `EXPORT bool obs_source_muted(const obs_source_t *source);`
- `libobs/obs.h:1333` — `EXPORT void obs_source_set_muted(obs_source_t *source, bool muted);`
- `libobs/obs-source.c:299` shows `obs_source_muted()` is checked during push-to-talk handling; mute state is source-level, so it can be mirrored as a control event without touching the audio transport.

**Audio capture (exploratory path)**

- `libobs/obs.h:1347` — `typedef void (*obs_source_audio_capture_t)(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted);`
- `libobs/obs.h:1352–1354` — add/remove capture callback. `libobs/obs-source.c:5572` shows registration is mutex-protected (`audio_cb_mutex`), so registration/removal is thread-safe.
- `libobs/obs-source.c:1522–1530` — `source_signal_audio_data()` iterates the callback list; `obs-source.c:1696` shows it is called from `obs_source_output_audio()`. Callback invocation is on the **OBS audio thread**, so it must only enqueue to a bounded queue.
- Observed gap: three of five bound sources emitted callbacks and two did not. The exact cause is still being verified (OBS 33 monitoring-dedup logic is mentioned in the sender log for the WASAPI output sources). This is the strongest argument for making mixer-track audio authoritative.

**Output tracks (recommended path)**

- `libobs/obs.h:2036/2039/2042/2045` — `obs_output_set_mixer`, `get_mixer`, `set_mixers`, `get_mixers`.
- `libobs/obs-output.h` — `raw_audio2` callback signature used by the committed code before the exploratory edit.
- `frontend/settings/OBSBasicSettings.cpp:2175` — `config_get_string(main->Config(), "AdvOut", "Track1Name")`; `:3583` — `SaveEdit(ui->advOutTrack1Name, "AdvOut", "Track1Name")`.
- `frontend/api/obs-frontend-api.h:210` — `EXPORT config_t *obs_frontend_get_profile_config(void);`
- Therefore **OBS's own track names are readable** via `obs_frontend_get_profile_config()` + `config_get_string(cfg, "AdvOut", "Track<N>Name")`; no extra Wormhole naming setting is required. This resolves open decision (2) — confirm the approach with the owner.

**Threading / lifecycle invariants**

- `libobs/obs.h:950–961` — `obs_task_t`, `enum obs_task_type { OBS_TASK_UI, OBS_TASK_GRAPHICS, OBS_TASK_AUDIO, OBS_TASK_DESTROY }`, `obs_queue_task(type, task, param, wait)`, `obs_in_task_thread(type)`. UI-thread mutation must go through `OBS_TASK_UI`; using `wait=true` from the sender worker thread blocks that worker, not OBS's UI, but must still be bounded.
- `libobs/obs.c:3253` — `obs_video_active()` returns true while any canvas has `raw_active > 0`.
- `libobs/obs-output.c:2758` — `obs_output_begin_data_capture()` sets `data_active`/`active`; `obs-output.c:2875` — `obs_output_end_data_capture_internal()` starts the async end-capture thread. libobs does **not** end raw capture in the normal `obs_output_stop()` path; exp_016 added the explicit call and it must stay.
- `include/wormhole/protocol_v2.h:16–18` — `WH2_CAP_MULTI_AUDIO=1`, `SIMD_JPEG=2`, `CLOCK_SYNC=4`; next free bit is `1<<3` for scene sync.

## 6. UI design (pure C, cyberpunk styling retained)

- **Sender Tools menu**: `Wormhole...` (settings, unchanged) plus new
  `Wormhole Sync...` showing a panel with `> PUSH SCENE SYNC` button, the
  active scene name, track/channel count, and the existing status button.
- **Receiver source properties**: replace the reconnect-based `SYNC SCENES`
  button with a real `> REQUEST SCENE SYNC`; keep `SCAN NETWORK` and manual
  host/port. Show `linked to <sender name>` and the mirrored scene count.
- Mirrored sources appear in the receiver's Sources list with their sender
  names, so mute/hide/switch controls in OBS behave like the gaming PC.
- No Qt/C++ — OBS properties API only, as required by `GOAL.md`.

## 7. Failure modes and invariants (must preserve)

- `STOP_COUNT=1` on disable; no double stop/join; `shutdown()` before `close()`
  (exp_016).
- `obs_video_active()` transitions `1 → 0` when disabled; Output Mode stays
  editable (exp_016).
- Receiver auto-reconnect must reset manifest/clone state and rebuild without
  duplicates.
- Clone cleanup must enumerate **all** scenes, not just the current one.
- Refcount discipline: `obs_source_get_ref` for bindings/clones, `bfree` for
  `bzalloc`, `obs_data_release`, no CRT `free()` on OBS allocations.
- Never call OBS mutation APIs from worker/audio threads; never block an OBS
  thread on network I/O.
- Bounded queues and bounded manifest buffers; validate all frame lengths.
- Stale clones from a previous session must not intercept audio.

## 8. Test and evidence plan (`exp_018`)

- `exp_018/HYPOTHESIS.md` written first: one video signal, N track channels,
  mirror state sync, mute/visibility/scene-switch propagation.
- Build: `cmd /c scripts\build_c.bat` (all C tests) and
  `cmd /c scripts\build_plugin.bat` (`/W4 /WX`).
- Runtime: `powershell -File scripts\setup_portable_obs.ps1`, launch both via
  OBS-MCP config `E:\__PROJECTS__\OBS-MCP\config\gaming-streaming.toml`.
- Prove with logs + WebSocket queries:
  - sender logs `raw_audio2` track descriptors for tracks 1 and 2 (not 5
    per-source callbacks);
  - receiver `GetInputList` shows `wormhole-audio` clones named after the
    sender's tracks and `GetSceneItemList` shows mirrored items;
  - `SetInputMute` / sceneitem visibility change on sender produces
    `[wormhole] control event ... applied` on receiver and matching OBS state;
  - `SCENE_SWITCH` changes the receiver's current scene;
  - disable → `STOP_COUNT=1`, `obs_video_active=0`, OBS alive; re-enable →
    receiver relinks and re-syncs;
  - `GetStats` on both instances shows no crash and stable CPU.
- Record all results, including failures, in `exp_018/FINDINGS.md`; update
  `PROTOCOL.md`, `PERFORMANCE_TARGETS.md`, `ARCHITECTURE_LOG.md`, `PROGRESS.md`,
  `RISKS.md` (resolve R08/R09) in the same commit series; PR with CI green.

## 9. Risks and open decisions (owner input needed)

1. **Audio model**: track mode (recommended) vs source mode vs both. This
   decides issues #42/#43/#44 and the fate of the uncommitted `output.c`.
2. **Track names**: research resolved this — `obs_frontend_get_profile_config()`
   (`obs-frontend-api.h:210`) plus `[AdvOut] Track<N>Name`
   (`OBSBasicSettings.cpp:2175/3583`) gives OBS's own names, so no extra
   Wormhole setting is needed. Confirm we should use them verbatim.
3. **Control authority**: one-way (gaming PC authoritative) initially, or
   bidirectional mute/hide/switch from either side? One-way is recommended.
4. **Mirroring side effects**: creating scenes/sources on the receiver changes
   its scene collection; confirm this is acceptable and define cleanup on
   disconnect.
5. **Dedup sources**: if track mode is chosen, per-source clones become
   opt-in; confirm that losing per-source names in the default path is OK.
6. **PROTOCOL.md** is still a draft and not locked in `ARCHITECTURE_LOG.md`;
   locking the new control messages may require an architecture decision entry.
7. No authentication — LAN only; acceptable per existing docs.

## Highest-priority first step

Write `exp_018/HYPOTHESIS.md` resolving decision (1) in favour of **output-track
audio channels with one-way scene/control mirroring**, and record the
per-source exploratory results (including the manifest-colon bug) as evidence
in `exp_017/FINDINGS.md` — then get the owner's confirmation on the six open
decisions before creating `feat/scene-mirror` from `a972bdb`.
