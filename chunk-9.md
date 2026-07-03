# Chunk 9: Linux Backend

## Goal

Add the planned Linux backend behind the existing `Memmy_Backend` interface
without changing portable core or CLI code.

## Spec Coverage

- Section 6: backend boundary and portable OS rules.
- Section 19: Linux backend plan.
- Section 24: API shape must not be Windows-only.

## Steps

1. Add `memmy/src/platform/linux/` source files matching the backend operation
   split used by Windows.
2. Implement Linux backend initialization for `Memmy_Context_InitDefault` when
   building on Linux.
3. Implement process enumeration using `/proc`.
4. Implement process path/name lookup using `/proc/<pid>/` data.
5. Implement module and region enumeration from `/proc/<pid>/maps`.
6. Implement process open/close using backend-private state appropriate for
   Linux.
7. Implement remote reads using `process_vm_readv` where available.
8. Implement remote writes using `process_vm_writev` where available.
9. Add `/proc/<pid>/mem` or `ptrace` fallback only where necessary and keep
   permission failures clear.
10. Map Linux failures into existing `Memmy_Status` values.
11. Ensure Linux backend capabilities accurately reflect available operations.
12. Update CMake with platform-conditional compilation.

## Tests

1. Existing pure-core and CLI parser tests pass unchanged on Linux.
2. Linux integration smoke test:
   - `memmy procs`
   - `memmy mods --pid <current-process-pid>`
   - safe read from a known current-process address when practical
3. Region mapping tests using representative `/proc/<pid>/maps` fixtures.
4. Permission-denied tests where practical in CI or documented manual checks.

## Done When

- Linux builds without Windows headers or Windows-only assumptions.
- The same CLI commands work against the Linux backend where permissions allow.
- Unsupported or permission-limited operations return clear `Memmy_Error`
  diagnostics.
