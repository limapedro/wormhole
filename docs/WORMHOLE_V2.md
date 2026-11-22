# Wormhole protocol v2 — design

## Why v2 exists

Teleport's wire format (PROTOCOL.md Part 1) is deliberately minimal: a 16-byte
header, no version, no total length, no sequence number, one undifferentiated
PCM stream and no capability negotiation. It cannot carry multiple audio
tracks and a legacy peer cannot tell a new message from a corrupt JPEG.

Wormhole v2 keeps full compatibility with Teleport while adding the missing
mechanisms. It is an evolution, not a fork: **compatibility mode is the
default fallback, v2 is negotiated when both peers support it**.

## Compatibility strategy

Teleport's receiver dials the sender. A Wormhole source does the same.

1. Immediately after connecting, a Wormhole receiver sends a v2 `HELLO`.
2. A Teleport sender treats it as an unknown packet type, discards the payload
   and continues sending ordinary `JPEG`/`WAVE` packets. The Wormhole receiver
   detects that no `HELLO_ACK` arrived within 250 ms and falls back to parsing
   Teleport packets. Result: **Wormhole receiving from Teleport works**.
3. A Teleport receiver simply connects to a Wormhole output and receives
   ordinary `JPEG`/`WAVE` packets. Result: **Teleport receiving from Wormhole
   works**.
4. Two Wormhole peers exchange `HELLO`/`HELLO_ACK` and switch to v2 frames.

No peer ever has to guess from byte patterns alone.

## v2 framing

Every v2 frame starts with a fixed 28-byte header:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | char[4] | `"WH02"` magic |
| 4 | 2 | uint16 LE | protocol version (2) |
| 6 | 2 | uint16 LE | message type |
| 8 | 4 | uint32 LE | total frame length including header |
| 12 | 4 | uint32 LE | sequence number |
| 16 | 8 | uint64 LE | timestamp, nanoseconds on the sender's shared clock |

Message types: `HELLO` 1, `HELLO_ACK` 2, `STREAM_DESC` 3, `VIDEO_FRAME` 16,
`AUDIO_FRAME` 17, `PING` 18, `GOODBYE` 19.

Unlike Teleport, `total_len` covers the whole frame, so an unknown message can
be skipped safely. All strings are length-prefixed (uint16 length + bytes).

## Multi-audio

`HELLO` carries: capability bitfield, sender name, track count, and video
format (width, height, fps numerator/denominator). `HELLO_ACK` carries the
accepted version and the capabilities both peers share.

For each audio track the sender emits a `STREAM_DESC` with:
- `track_id` (uint32, stable for the session),
- `name` (length-prefixed UTF-8),
- `sample_rate`, `channels`, `bits_per_sample`,
- `clock_id` (uint32; tracks sharing a clock are sample-accurate relative to
  each other),
- optional codec FourCC (0 = raw PCM).

`AUDIO_FRAME` then references `track_id` and carries its own nanosecond
timestamp plus interleaved PCM. A track can appear or disappear mid-session by
sending a new `STREAM_DESC` or `GOODBYE` for that track. Mixed sample rates are
legal because each frame carries its own rate.

Compatibility fallback: with a legacy peer, Wormhole sends only track 1 as an
ordinary Teleport `WAVE` packet using the global audio mix.

## Error handling rules

- A frame with bad magic, wrong version, impossible `total_len` (> 64 MiB) or
  a truncated payload is rejected; the connection is not killed for a single
  bad frame.
- Unknown message types are skipped using `total_len`.
- Every parser is fuzz-tested; no parser may read past the supplied buffer.
- No allocation occurs while parsing headers; payload buffers are owned by the
  caller.

## Status

Implemented: framing, HELLO/ACK, stream descriptors, video/audio frames,
unit tests. Integration into the libobs source/output is incremental; see
PROGRESS.md for verified milestones.
