# Chunk 6: Range Expressions And Pattern Scan

## Goal

Implement range expressions, pattern parsing/matching, chunked pattern scanning,
and the `pscan` command.

## Spec Coverage

- Section 13: pattern syntax, representation, parser, and matcher.
- Section 14: scan options, result lists, chunking rules, pattern scan API.
- Section 16: range expressions.
- Section 15.8-15.9 for shared scan result output shape, focused on `pscan`.
- Section 22 milestone 4.

## Steps

1. Implement `Memmy_Pattern_Parse` with exact bytes and wildcard support
   controlled by `Memmy_PatternParseFlag_AllowWildcards`.
2. Implement `Memmy_Pattern_Match` as a simple linear matcher.
3. Add `Memmy_Range`, `Memmy_RangeList`, and `Memmy_RangeList_Push`.
4. Implement `Memmy_RangeExpr_Resolve` for initial required forms:
   - `module`
   - `address_expr..address_expr`
   - `address_expr:+size`
   - `module[start..end]`
   - `module[start:+size]`
5. Reuse `Memmy_ConstExpr_ParseAndEval` for module bracket offsets and sized
   ranges.
6. Enforce half-open ranges and return overflow for invalid size/order
   arithmetic.
7. Implement `Memmy_ScanOptions`, `Memmy_ScanResult`,
   `Memmy_ScanResultList`, and push helpers.
8. Implement `Memmy_Process_ScanPattern`.
9. Scan only committed readable regions by default.
10. Apply access filters as intersections.
11. Read memory in chunks with overlap so matches crossing chunk boundaries are
    found.
12. Respect `chunk_size` and `max_results`.
13. Implement `memmy pscan --pid <pid> --pattern <pattern>` and
    `--range <range-expr>`.
14. Add `--readable`, `--writable`, and `--executable` filters.
15. For initial text output, print scan result addresses as fixed-width absolute
    addresses only. Module-relative display and JSONL formatting are added in
    Chunk 8.
16. Keep command execution data-oriented so Chunk 8 can add JSON/JSONL
    renderers without rewriting backend and parsing logic.

## Tests

1. Unit tests for pattern parsing with exact bytes, wildcards, invalid tokens,
   and wildcard rejection when flags are not set.
2. Unit tests for `Memmy_Pattern_Match`.
3. Unit tests for every initial range expression form.
4. Unit tests for range overflow and invalid half-open bounds.
5. Test-backend scan tests for:
   - default readable-region selection
   - access filter intersection
   - chunk boundary matches
   - max result limiting
   - explicit range limiting
6. CLI tests for `pscan` text output.

## Done When

- `pscan` can scan fake backend memory and real readable Windows regions.
- Pattern scan results are absolute addresses ready for later formatting and
  JSON output.
- The build and unit tests pass.
