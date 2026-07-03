# Chunk 9: Windows Backend Hardening And Integration

## Goal

Finish and harden the Windows backend required by v0, then verify the CLI
against real local processes where safe.

Linux and macOS remain future targets in v0; this chunk must not expand the
platform scope beyond Windows.

## Spec Coverage

- `spec-v0.md` section 8: backend boundary and capability accuracy.
- `spec-v0.md` section 9: process, module, region, read, and write APIs.
- `spec-v0.md` section 11: scan behavior with and without `ListRegions`.
- `spec-v0.md` section 12: command backend-capability and process-access
  requirements.
- `spec-v0.md` section 14: Windows backend APIs and cross-bitness support.

## Steps

1. Audit all production includes and confirm native Windows headers appear only
   under `memmy/src/platform/win32/`.
2. Verify the Win32 backend capability bitset accurately reflects implemented
   operations:
   - `Read`
   - `Write`
   - `ListProcs`
   - `ListModules`
   - `ListRegions`
3. Verify access mapping for:
   - `Memmy_ProcessAccess_Read`
   - `Memmy_ProcessAccess_Write`
   - `Memmy_ProcessAccess_Query`
4. Verify process enumeration reports stable pid, name, path, and pointer-width
   data where available.
5. Verify module enumeration reports name, path, base, and size.
6. Verify region enumeration reports base, size, access, and state without
   wrapping `base + size` in CLI output.
7. Verify same-bitness targets work.
8. Verify 64-bit hosts can inspect 64-bit and WOW64 32-bit targets.
9. Return `Memmy_Status_Unsupported` with a clear diagnostic for unsupported
   cross-bitness cases.
10. Harden read/write error mapping for common Windows failures:
    - access denied
    - partial copy
    - invalid or unreadable address
    - invalid or unwritable address
    - other platform failures with `os_code`
11. Verify scan chunk reads behave correctly when regions are readable,
    unreadable, guard, reserved, or free.
12. Confirm Windows backend code performs pointer casts only at the backend
    boundary.
13. Add integration tests or documented smoke-test commands that are safe on a
    developer machine.

## Tests

1. Full unit test suite passes on Windows.
2. Windows integration smoke tests:
   - `memmy procs`
   - `memmy mods --pid <current-process-pid>`
   - `memmy regions --pid <current-process-pid>`
3. Safe current-process fixture tests for:
   - `peek --addr`
   - `poke --addr --dry-run`
   - normal `poke` against controlled writable memory
   - `pscan --start ... --length ...`
   - `scan --start ... --length ...`
4. Manual or automated verification for WOW64 pointer width when available.
5. Negative tests for access-denied and unsupported cases where practical.

## Done When

- The Windows backend satisfies every v0 backend requirement.
- Real-process smoke tests pass or have documented platform/permission reasons
  for any skipped case.
- No future-platform implementation has been added.
- The build and unit tests pass before starting Chunk 10.
