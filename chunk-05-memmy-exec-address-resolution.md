# Chunk 05: Address Resolution

## Goal

Implement process-relative address resolution in `memmy_exec` for absolute and
module-based address expressions.

## Scope

- Resolve absolute address bases.
- Resolve module target bases against a caller-provided `Memmy_ModuleList`.
- Implement checked add/sub address arithmetic.
- Return resolved absolute `Memmy_Addr` results.
- Return `Memmy_Status_NotFound` or `Memmy_Status_Ambiguous` consistently for
  module lookup failures.
- Preserve process selector consistency checks at the boundary where
  `memmy_cli` provides the open process.

## Out of Scope

- Pointer-chain dereference resolution.
- Peeks, pokes, scans.
- Whole-process ranges.
- CLI wiring.

## Tests

Add `unittest/memmy/test_memmy_exec_address.c`.

Required tests:

- Resolves absolute addresses.
- Resolves module bases.
- Resolves module plus/minus offsets.
- Detects arithmetic overflow/underflow.
- Reports missing module.
- Reports ambiguous module if supported by module lookup behavior.
- Confirms no process enumeration happens in `memmy_exec`.

## Completion Criteria

- `memmy_exec` can resolve:

```txt
0x000001d856780004
<client.dll>+0x123
```

- `ctest --test-dir build` passes.
