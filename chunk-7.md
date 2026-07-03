# Chunk 7: Explicit-Range Pattern Scan

## Goal

Implement chunked byte-pattern scanning over exactly one caller-provided
absolute range, then expose it through `pscan`.

## Spec Coverage

- `spec-v0.md` section 6: single half-open ranges.
- `spec-v0.md` section 10: patterns and wildcard parsing.
- `spec-v0.md` section 11: `Memmy_ScanOptions`,
  `Memmy_Process_ScanPattern`, chunking, limits, and optional region
  intersection.
- `spec-v0.md` section 12.7: `pscan`.
- `spec-v0.md` section 13: scan text and JSONL address formatting.
- `spec-v0.md` section 16 milestone 6.

## Steps

1. Define:
   - `Memmy_ScanOptions`
   - `Memmy_ScanResult`
   - `Memmy_ScanResultList`
2. Implement scan-result list push helpers allocated from the output arena.
3. Implement `Memmy_Process_ScanPattern`.
4. Require callers to pass one explicit `Memmy_Range` in
   `Memmy_ScanOptions.range`.
5. Ensure all reads stay inside the specified range.
6. Support zero-length ranges by returning an empty result list.
7. Implement chunked reads with overlap so matches crossing chunk boundaries
   are found.
8. Respect:
   - `options->limit` as a maximum result count, where zero means the chosen
     default/unlimited behavior documented in code
   - `options->chunk_size`, with a tested default when zero
9. If `ListRegions` is available, optionally intersect the requested range with
   committed readable regions to avoid impossible reads.
10. If `ListRegions` is unavailable, scan by attempting chunked reads directly
    in the requested range and use this v0 scanner error policy:
    - if no bytes in the requested non-empty range can be read, return
      `Memmy_Status_Unreadable`
    - if some chunks are unreadable but scanning can continue, skip unreadable
      chunks and continue
    - if a read partially succeeds, scan the bytes that were read and continue
      unless the backend reports a fatal error
    - preserve any results found before a later fatal read error is returned
11. Do not create implicit input ranges from region enumeration.
12. Implement CLI:
    - `memmy pscan --pid <pid> --start <addr> --end <addr> --pattern <pattern>`
    - `memmy pscan --pid <pid> --start <addr> --length <size> --pattern <pattern>`
    - optional `--limit <count>`
    - optional `--chunk-size <bytes>`
13. Parse `--pattern` with `Memmy_PatternParseFlag_AllowWildcards`.
14. Require `Memmy_BackendCap_Read` and `Memmy_ProcessAccess_Read`.
15. Do not add `--range`, `--readable`, `--writable`, `--executable`, or
    module-relative result columns in v0.

## Tests

1. Unit tests for pattern scanning at the beginning, middle, and end of a fake
   memory range.
2. Unit tests proving scans never read outside the requested range.
3. Unit tests proving zero-length ranges produce no results.
4. Unit tests proving chunk-boundary matches are found.
5. Unit tests proving `limit` caps result count.
6. Unit tests for behavior with and without `ListRegions` capability.
7. Unit tests proving unreadable holes are skipped when other bytes in the
   requested range can be scanned.
8. Unit tests proving an entirely unreadable non-empty range returns
   `Memmy_Status_Unreadable`.
9. Unit tests proving partial reads are scanned for matches.
10. Unit tests proving free, reserved, guard, and unreadable regions are not used
   when region intersection is enabled.
11. CLI tests for both range option forms.
12. CLI tests for wildcard `pscan` patterns.

## Done When

- `pscan` scans one explicit absolute range and prints absolute result
  addresses.
- No implicit whole-process scan exists.
- The build and unit tests pass before starting Chunk 8.
