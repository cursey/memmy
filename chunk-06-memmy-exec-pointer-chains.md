# Chunk 06: Pointer-Chain Resolution

## Goal

Resolve `->` pointer-chain operations in `memmy_exec`.

## Scope

- Implement pointer reads for address expression resolution using
  `Memmy_Process_Read` or a core pointer-read helper.
- Honor target process pointer width:
  - 32-bit reads `U32` and zero-extends.
  - 64-bit reads `U64`.
- Resolve:
  - `addr->`
  - `addr->offset`
  - chained combinations of deref, add, and sub.
- Preserve checked arithmetic after pointer reads.

## Out of Scope

- Peek/poke expression execution.
- Range resolution.
- CLI wiring.

## Tests

Extend `unittest/memmy/test_memmy_exec_address.c` or add
`unittest/memmy/test_memmy_exec_pointer.c`.

Required tests:

- Resolves bare dereference.
- Resolves dereference plus offset.
- Resolves chained dereferences.
- Uses 32-bit pointer width correctly.
- Uses 64-bit pointer width correctly.
- Propagates unreadable or partial-read failures.
- Detects arithmetic overflow after pointer read.

## Completion Criteria

- `memmy_exec` can resolve:

```txt
<client.dll>+0x123->0x8
```

- `ctest --test-dir build` passes.
