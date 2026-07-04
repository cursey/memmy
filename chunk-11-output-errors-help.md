# Chunk 11: Output, Errors, Help, and Final v1 Audit

## Goal

Harden `--expr` output, errors, help text, and final spec conformance.

## Scope

- Add `--expr` usage to CLI help.
- Ensure `--expr` errors use stable `Memmy_Error` fields:
  - context
  - input
  - byte_offset
  - byte_count
  - message
- Ensure JSON failures use the standard failure object.
- Ensure top-level address expression JSON output is:

```json
{
  "address": "0x00007ff800004242"
}
```

- Ensure peek, poke, pattern scan, and value scan expressions reuse existing v0
  command-shaped output.
- Audit all v1 non-goals:
  - no REPL
  - no RHS expression evaluation
  - no ordering comparisons
  - no array views
  - no disassembly
  - no `address_expr..address_expr`
- Audit dependency boundaries:
  - `memmy_expr` does not depend on `memmy_exec`, `memmy_cli`, or `cmd_memmy`.
  - `memmy_exec` does not depend on `memmy_cli` or `cmd_memmy`.
  - `cmd_memmy` remains thin.

## Tests

Add or extend CLI tests for:

- Help includes top-level `--expr`.
- Help makes clear that top-level `--expr` does not accept scan tweakables such
  as `--limit` or `--chunk-size` in v1.
- Subcommands reject `--expr`.
- Top-level `--expr` rejects `--limit` and `--chunk-size` in v1.
- Parse errors include stable offset/count.
- JSON failures match standard shape.
- JSONL scan outputs remain stable.
- Bare whole-process targets such as `<game.exe!>` produce a clear error unless
  used as part of a pattern scan or value scan.
- All required acceptance examples pass.

## Completion Criteria

- `cmake -S . -B build` succeeds from a clean build directory.
- `cmake --build build` succeeds.
- `ctest --test-dir build` passes.
- All required acceptance examples from `spec-v1.md` work.
- Final manual audit confirms the repo satisfies `spec-v1.md`.
