# Chunk 08: Expression Peek and Poke

## Goal

Execute expression peeks and pokes through `memmy_exec`, then wire them through
top-level `memmy --expr`.

## Scope

- In `memmy_exec`, execute:
  - `address_expr : type`
  - `address_expr : type = value`
- Encode RHS value literals with `Memmy_Value_Parse` in `memmy_exec`, where the
  target process pointer width is known.
- Use existing core memory read/write behavior.
- Preserve v0 output semantics by returning structured results that
  `memmy_cli` can format like `peek` and `poke`.
- In `memmy_cli`, format expression peek/poke text and JSON using existing v0
  command-shaped output.

## Out of Scope

- Range scans.
- RHS expression evaluation.
- Array views.

## Tests

Add `unittest/memmy/test_memmy_exec_peek_poke.c` and extend CLI expression
tests.

Required tests:

- Executes absolute-address peek.
- Executes module-address peek.
- Executes pointer-chain peek.
- Executes absolute-address poke.
- Executes module-address poke.
- Rejects RHS address expressions.
- Propagates partial read/write and access errors.
- CLI text/JSON output matches existing peek/poke style.

## Completion Criteria

- These expressions work:

```txt
0x000001d856780004 : u32
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : i32 = 77
```

- `ctest --test-dir build` passes.
