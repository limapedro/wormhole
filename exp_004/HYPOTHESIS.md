# exp_004 — Hypothesis: differential capture comparison

**Status:** hypothesis written before the formal experiment run.

## What we expect

1. Given two byte-identical captures, the differential tester reports
   `identical = true`, equal packet counts and no differences, and exits
   successfully.
2. Changing a single timestamp byte in one capture is detected as a timestamp
   difference at the correct packet index, and the command exits with an
   error.
3. The JSON report records both capture paths, packet counts and the precise
   differences.

## Why

The differential tester is the gate that will eventually compare original
Teleport and Wormhole outputs. Before it is trusted with that job, it must be
shown to accept identical streams and reject a known, controlled difference.
A tester that always reports success would silently invalidate every later
parity claim.

## What would prove us wrong

- Identical captures reported as different (false positive).
- A modified timestamp not reported (false negative).
- Wrong packet index or missing fields in the JSON report.

## Method

- Generate a deterministic capture with `synth`.
- Copy it to two paths and run `differential`; expect success.
- Flip one byte inside the first packet's 64-bit timestamp in the right-hand
  copy and rerun; expect a reported timestamp difference and a non-zero exit.
- Inspect the JSON report in both cases.

## Notes

This tests the comparison stage only. It does not compare real Teleport with
Wormhole, and no parity or performance claim is made.
