# Chunk 2: Process, Module, And Region Core

## Goal

Implement the portable process, module, and region APIs on top of
`Memmy_Backend`, with list helpers and lookup behavior fully testable through
the test backend.

## Spec Coverage

- Section 7: processes and process access.
- Section 8: modules and module lookup.
- Section 9: memory regions and region predicates.
- Section 10.3: pointer-width-aware pointer reads.

## Steps

1. Add public process declarations:
   - `Memmy_ProcessInfo`
   - `Memmy_ProcessList`
   - `Memmy_ProcessAccess`
   - `Memmy_ListProcesses`
   - `Memmy_ProcessList_Push`
   - `Memmy_Process_Open`
   - `Memmy_Process_IsOpen`
   - `Memmy_Process_Close`
2. Implement process functions as backend dispatch through the current
   `Memmy_Context` for top-level operations.
3. Ensure `Memmy_Process_Open` stores the selected backend pointer in the
   arena-owned `Memmy_Process`.
4. Add public module declarations:
   - `Memmy_Module`
   - `Memmy_ModuleList`
   - `Memmy_Process_ListModules`
   - `Memmy_ModuleList_Push`
   - `Memmy_ModuleList_FindByName`
   - `Memmy_ModuleList_FindByAddress`
5. Implement module functions as process-bound backend dispatch.
6. Implement module lookup rules:
   - lookup by `Memmy_Module.name`, not path
   - not found returns `Memmy_Status_NotFound`
   - duplicate matches return `Memmy_Status_Ambiguous`
   - address lookup uses half-open `[base, base + size)` ranges
   - overflow in `base + size` returns `Memmy_Status_Overflow`
7. Add public region declarations:
   - `Memmy_RegionAccess`
   - `Memmy_RegionState`
   - `Memmy_Region`
   - `Memmy_RegionList`
   - `Memmy_Process_ListRegions`
   - `Memmy_RegionList_Push`
   - `Memmy_RegionList_FindByAddress`
   - `Memmy_Region_IsReadable`
   - `Memmy_Region_IsWritable`
   - `Memmy_Region_IsExecutable`
8. Implement region lookup and predicates, including guard/free/reserved
   rejection even when access bits are present.
9. Implement `Memmy_Process_Read`, `Memmy_Process_Write`, and
   `Memmy_Process_ReadPtr` as process-bound backend dispatch.
10. Extend the test backend helpers so tests can add processes, modules,
    regions, memory, and pointer-width settings without platform dependencies.

## Tests

1. Unit tests for process list push and backend list dispatch.
2. Unit tests for open/close/is-open, including repeated close.
3. Unit tests for module lookup by name, not found, ambiguous, and address.
4. Unit tests for region lookup, overlapping ambiguity, and zero-size rejection
   if the helper attempts to add invalid regions.
5. Unit tests for readable/writable/executable predicates.
6. Unit tests for 32-bit and 64-bit `Memmy_Process_ReadPtr`.

## Done When

- All APIs in sections 7-10.3 compile and are exercised through the test backend.
- Remote addresses remain `Memmy_Addr` values outside platform code.
- The build and unit test executable pass.
