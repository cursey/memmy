# Chunk 02: Address Expression Parser

## Goal

Extend `memmy_expr` to parse v1 address expressions without resolving them.

## Scope

- Parse address bases:
  - absolute integer addresses
  - target refs from chunk 01
- Parse address operations:
  - `+offset`
  - `-offset`
  - `->`
  - `->offset`
- Support integer offsets and parenthesized constant-expression offsets.
- Preserve v1 whitespace rules:
  - no whitespace inside address expressions except inside parenthesized
    constant expressions.
- Store parsed operations in an arena-owned representation usable by
  `memmy_exec`.

## Required Examples

```txt
0x000001d856780004
<client.dll>+0x123
<client.dll>+0x123->0x8
<client.dll>+0x4242->(8 * 0x30)->(-0x4)
```

## Out of Scope

- Pointer-chain resolution.
- Module lookup.
- Top-level `: type`, `= value`, `{ pattern }`, or scan forms.

## Tests

Add `unittest/memmy/test_memmy_expr_address.c`.

Required tests:

- Parses absolute address bases.
- Parses module target bases.
- Parses add and sub operations.
- Parses deref and deref-offset operations.
- Parses parenthesized negative offsets.
- Rejects invalid whitespace.
- Rejects malformed pointer-chain syntax.
- Rejects `address_expr..address_expr` ranges as not in v1.

## Completion Criteria

- Address-expression parser returns stable parsed structures and useful
  `Memmy_Error` locations.
- `ctest --test-dir build` passes.
