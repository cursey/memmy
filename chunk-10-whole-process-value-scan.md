# Chunk 10: Whole-Process Ranges and Value Scans

## Goal

Support whole-process expression ranges and exact value scan expressions.

## Scope

- In `memmy_exec`, resolve whole-process ranges from:
  - `<game.exe!>`
  - `<123!>`
- Require regions for whole-process ranges.
- Execute `range_expr : type == value` as an exact encoded-byte scan using the
  existing value scan machinery.
- In `memmy_cli`, collect region lists when requirements say they are needed.
- Format value scan results like `scan`.
- Expression value scans use default scan options in v1.

## Out of Scope

- Ordering comparisons.
- Repeated/narrowing scans.
- Region filter syntax in expressions.
- Tweakable scan options for top-level `--expr`, including `--limit` and
  `--chunk-size`.

## Tests

Add `unittest/memmy/test_memmy_exec_value_scan.c`; extend CLI expression tests.

Required tests:

- Whole-process range requires regions.
- Whole-process target is invalid for peek/poke/address-only expressions.
- Bare whole-process target is invalid as a top-level patternless range
  expression, such as `memmy --expr "<game.exe!>"`.
- Exact value scan over a module range.
- Exact value scan over an address-sized range.
- Exact value scan over whole process.
- Rejects `<game.exe!> : i32 > 42`.
- Uses default scan options for expression value scans.
- CLI text/JSONL output matches `scan` style.

## Completion Criteria

- This expression works:

```txt
<game.exe!> : i32 == 42
```

- `ctest --test-dir build` passes.
