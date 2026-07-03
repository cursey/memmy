# Chunk 4: Address Expressions And Memory Read

## Goal

Implement constant expressions, address expressions, pointer-chain resolution,
memory reads, and the `addr` and `peek` commands.

## Spec Coverage

- Section 10.1 and 10.3: read and pointer read.
- Section 11: address expression grammar, parser, resolver, and trace.
- Section 12 type parsing for read-sized scalar and payload requests.
- Section 15.5-15.6: `addr` and `peek`.
- Section 22 milestone 2.

## Steps

1. Implement `Memmy_ConstExpr_ParseAndEval` using checked arithmetic helpers
   from `base_checked.h`.
2. Add parser tests for integer forms, unary operators, binary precedence,
   parentheses, division/modulo by zero, and overflow.
3. Implement `Memmy_AddressExpr_Parse` with:
   - integer base
   - bracketed module base
   - `+offset`
   - `-offset`
   - bare `->`
   - `->offset`
   - parenthesized constant offsets
4. Preserve operation source text for resolver traces.
5. Implement `Memmy_AddressExpr_Resolve` using module lookup and
   `Memmy_Process_ReadPtr`.
6. Use checked address arithmetic for every add/subtract/deref-offset step.
7. Return precise `Memmy_Error` parser spans for invalid syntax.
8. Implement `Memmy_Type_Parse` for all spellings, with `ptr` size determined
   at concrete use sites by target pointer width.
9. Implement typed peek read helpers:
   - scalar integer and float reads
   - pointer reads
   - `bytes`, `str`, and `wstr` reads requiring `--count`
10. Validate UTF-8 for `str` and UTF-16LE for `wstr`.
11. Implement `memmy addr --pid <pid> --expr <expr>` with trace text output.
12. Implement `memmy peek --pid <pid> --expr <expr> --type <type>` and the
    payload `--count` forms.
13. Request minimal process access for these commands and require module
    listing only when the expression uses module syntax.
14. Keep command execution data-oriented so Chunk 8 can add JSON/JSONL
    renderers without rewriting backend and parsing logic.

## Tests

1. Unit tests for address expression parsing, invalid forms, and error spans.
2. Unit tests for resolver traces using the test backend.
3. Unit tests for 32-bit and 64-bit pointer-chain resolution.
4. Unit tests for read failures:
   - unreadable
   - partial read
   - overflow
   - missing or ambiguous module
5. CLI tests using the test backend or a local fixture process for `addr` and
   `peek`.
6. Windows smoke test reading from the current process where safe.

## Done When

- `addr` and `peek` satisfy their text-output forms.
- The address expression language is the shared resolver used by all commands
  that accept addresses.
- The build and unit tests pass.
