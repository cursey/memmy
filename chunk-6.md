# Chunk 6: Absolute Poke And Dry Run

## Goal

Implement remote memory writes and the v0 `poke` command using absolute
`--addr` input only, including dry-run old/new display.

## Spec Coverage

- `spec-v0.md` section 5: absolute address parsing.
- `spec-v0.md` section 9: `Memmy_Process_Write`.
- `spec-v0.md` section 10: value encoding.
- `spec-v0.md` section 12.5: `poke`.
- `spec-v0.md` section 14: Windows write backend.
- `spec-v0.md` section 16 milestone 5.

## Steps

1. Implement `Memmy_Process_Write` as process-bound backend dispatch.
2. Extend the test backend write behavior to support:
   - successful full writes
   - partial writes
   - unwritable ranges
   - access-denied cases
   - proof that dry-run leaves memory unchanged
3. Implement Windows backend write using `WriteProcessMemory` or the chosen v0
   write API.
4. Do not change remote memory protection in v0.
5. Map backend write failures to:
   - `Memmy_Status_Unwritable`
   - `Memmy_Status_PartialWrite`
   - `Memmy_Status_AccessDenied`
   - `Memmy_Status_PlatformError`
6. Implement:
   - `memmy poke --pid <pid> --addr <addr> --type <type> --value <value>`
   - `memmy poke --pid <pid> --addr <addr> --type u32 --value 1337 --dry-run`
7. Support all value types from `Memmy_Value_Parse`, including `bytes`, `str`,
   and `wstr`.
8. For dry-run, read the old bytes, format old/new values, and perform no
   write.
9. For normal poke, read the old value first for display consistency, then write
   the encoded new value.
10. Request `Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write` and require
    `Memmy_BackendCap_Read | Memmy_BackendCap_Write`.
11. Keep `poke` independent of region enumeration.
12. Do not support `--expr`, module-relative input, pointer chains, remote
    allocation, or memory protection changes.

## Tests

1. Unit tests for `Memmy_Process_Write` dispatch through the test backend.
2. Unit tests for full write, partial write, unwritable range, and
   access-denied mapping.
3. CLI tests for dry-run proving memory is unchanged.
4. CLI tests for normal `poke` proving memory changes in the test backend.
5. CLI tests for all representative value classes:
   - scalar integer
   - pointer
   - bytes
   - UTF-8 string
   - UTF-16LE string
6. CLI tests proving `--addr` rejects expression-like input.
7. Windows smoke test writing to controlled current-process memory where safe.

## Done When

- `poke` and `poke --dry-run` satisfy the v0 absolute-address behavior.
- Write status mapping is stable and tested.
- The build and unit tests pass before starting Chunk 7.
