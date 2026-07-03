# Chunk 7: Value Scan

## Goal

Implement exact value scanning for scalar, byte, UTF-8, and UTF-16LE values,
then expose it through the `scan` command.

## Spec Coverage

- Section 12.1: scan value encoding.
- Section 14: value scan API and chunking.
- Section 15.8: `scan`.
- Section 22 milestone 5.

## Steps

1. Reuse `Memmy_Value_Parse` from chunk 5 for all scan values.
2. Implement `Memmy_Process_ScanValue` as an exact byte-sequence scan over the
   encoded `Memmy_Value.bytes`.
3. Share candidate-region enumeration, range limiting, access filters, chunk
   reads, overlap preservation, and result limiting with pattern scan.
4. Ensure string scans are case-sensitive and do not append implicit trailing
   NUL bytes.
5. Add `scan` CLI parsing:
   - `--pid`
   - optional `--range`
   - `--type`
   - `--value`
   - `--readable`
   - `--writable`
   - `--executable`
6. Require `Read`, `ListRegions`, and `ListModules` backend capabilities for
   scan commands as specified.
7. Use the same initial text output columns as `pscan`, with fixed-width
   absolute addresses only.
8. Keep scan result generation in the core library. Module-relative display and
   JSONL formatting are added in Chunk 8.
9. Keep command execution data-oriented so Chunk 8 can add JSON/JSONL renderers
   without rewriting backend and parsing logic.

## Tests

1. Unit tests for scalar value scanning at multiple alignments.
2. Unit tests for byte, UTF-8, and UTF-16LE value scans.
3. Unit tests for range-restricted scans.
4. Unit tests proving chunk boundary matches are found.
5. Unit tests proving unreadable, free, reserved, and guard regions are skipped.
6. CLI tests for representative `scan` invocations.

## Done When

- `scan` satisfies the spec text-output behavior.
- Pattern and value scanning share the same range and chunking semantics.
- The build and unit tests pass.
