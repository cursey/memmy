# Chunk 01: memmy_expr Foundation

## Goal

Add the `memmy_expr` library target and the first parser layer for target refs
and constant expressions. This chunk should not execute or resolve memory.

## Scope

- Add `memmy_expr/` with:
  - `memmy_expr/CMakeLists.txt`
  - `memmy_expr/include/`
  - `memmy_expr/src/`
- Add `memmy_expr` after `memmy` and before later v1 libraries in the top-level
  CMake order.
- Link `memmy_expr` against `memmy`.
- Expose a public aggregate header, such as `memmy_expr.h`.
- Define parsed data structures for:
  - process selectors
  - target refs
  - constant expressions, or evaluated constant-expression results
- Parse target refs:
  - `<client.dll>`
  - `<game.exe!client.dll>`
  - `<123!client.dll>`
  - `<game.exe!>`
  - `<123!>`
- Preserve the rule that `<123>` is a module name, not a PID target.
- Parse and evaluate parenthesized constant expressions used by v1 grammar.

## Out of Scope

- Address-expression parsing beyond a bare target ref.
- Top-level memory-expression parsing.
- Process enumeration or process opening.
- Address/range resolution.
- Reads, writes, scans, or output formatting.

## Tests

Add `unittest/memmy/test_memmy_expr_target.c` and/or
`unittest/memmy/test_memmy_expr_const.c`.

Required tests:

- Parses unqualified module targets.
- Parses process-name-qualified module targets.
- Parses PID-qualified module targets.
- Parses whole-process targets.
- Rejects empty target parts.
- Rejects `<`, `>`, and `!` inside names where invalid.
- Confirms `<123>` is parsed as a module target.
- Evaluates supported constant expressions with precedence.
- Rejects division by zero, modulo by zero, invalid syntax, and overflow.

## Completion Criteria

- `cmake -S . -B build` configures.
- `cmake --build build` builds `memmy_expr`.
- `ctest --test-dir build` passes.
- `memmy_expr` has no dependency on `memmy_cli` or `cmd_memmy`.
