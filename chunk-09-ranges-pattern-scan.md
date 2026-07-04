# Chunk 09: Range Resolution and Pattern Scans

## Goal

Resolve v1 range expressions and execute pattern scan expressions.

## Scope

- In `memmy_exec`, resolve:
  - module range: `<client.dll>`
  - module bracket range: `<client.dll>[0x1000..0x5000]`
  - module sized range: `<client.dll>[0x1000:+0x4000]`
  - address-sized range: `<client.dll>+0x123:+0x500`
  - absolute address-sized range: `0x1000:+0x500`
- Execute `range_expr { pattern }`.
- Pass `Memmy_PatternParseFlag_AllowWildcards` for expression pattern scans.
- In `memmy_cli`, format results like `pscan`.
- Expression pattern scans use default scan options in v1.

## Out of Scope

- Whole-process ranges.
- Value scans.
- `address_expr..address_expr` ranges.
- Tweakable scan options for top-level `--expr`, including `--limit` and
  `--chunk-size`.

## Tests

Add `unittest/memmy/test_memmy_exec_range.c` and
`unittest/memmy/test_memmy_exec_pattern_scan.c`; extend CLI expression tests.

Required tests:

- Resolves module full range.
- Resolves module bracket and sized ranges.
- Resolves module address-sized ranges.
- Resolves absolute address-sized ranges.
- Rejects `address_expr..address_expr`.
- Executes pattern scans with wildcards.
- Uses default scan options for expression pattern scans.
- CLI text/JSONL output matches `pscan` style.

## Completion Criteria

- This expression works:

```txt
<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}
```

- `ctest --test-dir build` passes.
