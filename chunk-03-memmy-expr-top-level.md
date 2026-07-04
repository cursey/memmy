# Chunk 03: Top-Level Memory Expression Parser

## Goal

Complete `memmy_expr` parsing for all v1 top-level expression shapes.

## Scope

- Define `Memmy_MemoryExpr` and `Memmy_MemoryExprKind`.
- Implement `Memmy_MemoryExpr_Parse`.
- Parse:
  - address expression
  - `address_expr : type`
  - `address_expr : type = value`
  - `range_expr { pattern }`
  - `range_expr : type == value`
- Parse v1 range expressions:
  - target ref as range
  - module bracket range: `<module>[start..end]`
  - module sized range: `<module>[start:+size]`
  - address-sized range: `address_expr:+size`
- Use existing `Memmy_Type_Parse` for view/type names.
- Store poke and value-scan RHS literals as parsed source slices or arena-owned
  text. Do not encode them as `Memmy_Value` in `memmy_expr`; `ptr` and other
  target-sensitive values need target process context and belong in
  `memmy_exec`.
- Pattern text may be parsed into `Memmy_Pattern` in `memmy_expr` because
  pattern parsing is not target-dependent.
- Lex `==` before `=`.

## Required Examples

```txt
0x000001d856780004 : u32
<client.dll>+0x123
<client.dll>+0x123->0x8
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : i32 = 77
<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}
<game.exe!> : i32 == 42
```

## Out of Scope

- Process selection.
- Address/range resolution.
- Reads, writes, scans, and output formatting.

## Tests

Add `unittest/memmy/test_memmy_expr_memory.c` and
`unittest/memmy/test_memmy_expr_range.c`.

Required tests:

- Dispatches each top-level kind correctly.
- Parses pattern scans with wildcards.
- Parses exact value scans with `==`.
- Rejects ordering comparisons.
- Rejects `address_expr..address_expr` ranges.
- Rejects bare whole-process targets as top-level expressions, such as
  `<game.exe!>` and `<123!>`.
- Rejects RHS address expressions for pokes.
- Ensures `=` is poke-only and `==` is scan-only.
- Validates byte offsets in parse errors.

## Completion Criteria

- All required acceptance expressions parse.
- Parser does not call backend, process, read, write, or scan APIs.
- `ctest --test-dir build` passes.
