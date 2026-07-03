# Chunk 8: Explicit-Range Value Scan

## Goal

Implement exact typed-value scanning over exactly one caller-provided absolute
range, then expose it through `scan`.

## Spec Coverage

- `spec-v0.md` section 6: single half-open ranges.
- `spec-v0.md` section 10: value parsing and bytes wildcard rejection.
- `spec-v0.md` section 11: `Memmy_Process_ScanValue`, chunking, limits, and
  optional region intersection.
- `spec-v0.md` section 12.6: `scan`.
- `spec-v0.md` section 13: scan text and JSONL address formatting.
- `spec-v0.md` section 16 milestone 7.

## Steps

1. Implement `Memmy_Process_ScanValue` as an exact byte-sequence scan over
   `Memmy_Value.bytes`.
2. Share the chunked scanner core with `Memmy_Process_ScanPattern` where it
   keeps behavior identical.
3. Reuse `Memmy_Value_Parse` for all scan values.
4. Ensure `bytes` scan values reject wildcard tokens.
5. Ensure `str` and `wstr` scans are case-sensitive and do not append implicit
   trailing NUL bytes.
6. Support scalar, pointer, bytes, UTF-8 string, and UTF-16LE string scans.
7. Preserve the v0 scan rules:
   - reads stay inside the requested range
   - matches may cross chunk boundaries
   - result addresses are absolute
   - zero-length ranges produce no results
   - region listing may optimize reads but must not create input ranges
   - unreadable-hole and partial-read behavior matches
     `Memmy_Process_ScanPattern`
8. Implement CLI:
   - `memmy scan --pid <pid> --start <addr> --end <addr> --type <type> --value <value>`
   - `memmy scan --pid <pid> --start <addr> --length <size> --type <type> --value <value>`
   - optional `--limit <count>`
   - optional `--chunk-size <bytes>`
9. Require `Memmy_BackendCap_Read` and `Memmy_ProcessAccess_Read`.
10. Do not require `ListRegions` for `scan`; use it only as an optimization
    when available.
11. Do not add implicit whole-process scans, narrowing scans, `--range`, or
    access filter options.

## Tests

1. Unit tests for scalar value scanning at multiple alignments.
2. Unit tests for pointer-width-aware `ptr` scans on 32-bit and 64-bit fake
   processes.
3. Unit tests for bytes, UTF-8, and UTF-16LE value scans.
4. Unit tests proving bytes values reject wildcards.
5. Unit tests for both explicit range forms.
6. Unit tests proving chunk-boundary matches are found.
7. Unit tests proving `limit` caps result count.
8. Unit tests for scan behavior with and without `ListRegions`.
9. Unit tests proving unreadable holes and partial reads match pattern-scan
   behavior.
10. CLI tests for representative `scan` invocations.

## Done When

- `scan` satisfies the v0 explicit-range behavior.
- Pattern and value scanning share range, chunking, and limit semantics.
- The build and unit tests pass before starting Chunk 8b.
