# exp_013 — Hypothesis: a C Wormhole plugin can load in OBS and a v2 protocol can exceed Teleport

**Status:** hypothesis written before the formal evidence summary.

## What we expect

1. A C plugin named `wormhole` can be built against OBS 33.0.0 headers and the
   `obs.dll` export table, producing `wormhole.dll` that libobs loads.
2. The plugin registers `wormhole-source` and `wormhole-output` and logs its
   version on load.
3. A versioned Wormhole v2 wire format can negotiate capabilities and carry
   three independently identified audio streams plus video, which Teleport's
   protocol cannot do.

## Why

The project still had no Wormhole plugin — only recon, harnesses and MCP. This
experiment establishes the build/load path for the actual plugin and the
protocol foundation that makes Wormhole "a step beyond Teleport" rather than a
renamed clone.

## What would prove us wrong

- The plugin fails to compile or OBS refuses to load it.
- v2 messages fail round-trip or the TCP negotiation fails.

## Method

- Download OBS 33.0.0 source headers into git-ignored `third_party/obs-sdk`,
  generate `obs.lib` from `obs.dll` exports with `dumpbin`/`lib`, compile
  `plugin/src/plugin.c` with MSVC `/W4 /WX /wd4201`.
- Copy `wormhole.dll` into a portable OBS plugin directory and inspect the log.
- Build and run `test_protocol_v2` and `test_v2_negotiation`.

## Notes

Informal runs occurred while developing; the formal evidence recorded in
FINDINGS.md is the verified state.
