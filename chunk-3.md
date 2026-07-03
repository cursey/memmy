# Chunk 3: Process, Module, Region APIs And Inventory CLI

## Goal

Implement the portable process/module/region APIs on top of `Memmy_Backend`,
add the first Windows backend inventory slice, and expose the v0 discovery
commands: `procs`, `mods`, and `regions`.

## Spec Coverage

- `spec-v0.md` section 8: backend dispatch and capability checks.
- `spec-v0.md` section 9: process, module, and region APIs.
- `spec-v0.md` section 12.1-12.3: `procs`, `mods`, and `regions` command
  behavior.
- `spec-v0.md` section 14: initial Windows backend APIs for enumeration.
- `spec-v0.md` section 16 milestone 1.

## Steps

1. Implement backend-dispatched public APIs:
   - `Memmy_ListProcesses`
   - `Memmy_Process_Open`
   - `Memmy_Process_IsOpen`
   - `Memmy_Process_Close`
   - `Memmy_Process_ListModules`
   - `Memmy_Process_ListRegions`
2. Add internal list push helpers for test/backend implementations if useful,
   keeping allocation through `Arena`.
3. Ensure `Memmy_Process_Open` stores the selected backend pointer in the
   arena-owned `Memmy_Process`.
4. Ensure `Memmy_Process_Close` is harmless when called repeatedly.
5. Extend the test backend so tests can configure:
   - multiple fake processes for `procs` and `--name`
   - fake process names and paths
   - fake modules
   - fake regions
   - pointer width per fake process
6. Implement `Memmy_Context_InitDefault` so Windows hosts install the Win32
   backend. Unsupported hosts may return `Memmy_Status_Unsupported` for v0.
7. Add Win32 backend files under `memmy/src/platform/win32/`.
8. Implement Windows process enumeration, process open/close, module
   enumeration, and region enumeration using documented APIs listed in
   `spec-v0.md`.
9. Map Windows region protection/state into `Memmy_RegionAccess` and
   `Memmy_RegionState`.
10. Implement CLI command dispatch and global option parsing skeleton:
    - `memmy --help`
    - `memmy --version`
    - `memmy procs`
    - `memmy mods --pid <pid>`
    - `memmy regions --pid <pid>`
11. Add `--filter` for `procs` and `mods`.
12. Print text output for `procs`, `mods`, and `regions` exactly in the v0
    table shape.
13. Compute region `END` as `base + size` with overflow detection; report
    `Memmy_Status_Overflow` rather than wrapping.
14. Check backend capabilities before performing command work.
15. Keep command handlers data-oriented so later JSON output can render the same
    typed result data.

## Tests

1. Unit tests for process list dispatch, process open/close, and repeated close.
2. Unit tests for module and region list dispatch through the test backend.
3. Unit tests for readable/writable/executable/guard/state mapping helpers if
   they are exposed internally.
4. CLI parser tests for `procs`, `mods`, `regions`, `--pid`, `--filter`,
   `--help`, and `--version`.
5. CLI tests with the test backend for text output of `procs`, `mods`, and
   `regions`.
6. CLI test proving region-end overflow reports `Memmy_Status_Overflow`.
7. Windows smoke tests:
   - `memmy --help`
   - `memmy procs`
   - `memmy mods --pid <current-process-pid>`
   - `memmy regions --pid <current-process-pid>`

## Done When

- The v0 discovery commands work in text mode.
- `cmd_memmy` builds and emits an executable named `memmy`.
- No CLI or core file includes Windows headers directly.
- The build and unit tests pass before starting Chunk 4.
