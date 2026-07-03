# Chunk 10: macOS Backend And Final Spec Audit

## Goal

Add the planned macOS backend and perform the final requirement-by-requirement
audit proving the implemented repo satisfies `spec.md`.

## Spec Coverage

- Section 20: macOS backend plan.
- Section 21: complete test strategy.
- Section 23: initial non-goals remain out of scope.
- Section 24: design principles.
- Whole-spec completion audit.

## Steps

1. Add `memmy/src/platform/macos/` source files matching the backend operation
   split used by Windows and Linux.
2. Implement macOS backend initialization for `Memmy_Context_InitDefault` when
   building on macOS.
3. Implement process enumeration using `proc_listpids` and process path lookup
   using `proc_pidpath`.
4. Implement process open/close around Mach task access where permitted.
5. Implement remote reads using `mach_vm_read_overwrite`.
6. Implement remote writes using `mach_vm_write`.
7. Implement region enumeration using `mach_vm_region`.
8. Map macOS permission, SIP, hardened runtime, and code-signing failures into
   clear `Memmy_Status_AccessDenied`, `Memmy_Status_Unsupported`, or
   `Memmy_Status_PlatformError` diagnostics.
9. Ensure macOS backend capabilities reflect actual runtime support.
10. Update CMake with platform-conditional macOS compilation.
11. Add macOS integration smoke tests or documented manual test commands.
12. Run a full audit against every `spec.md` section and record gaps as either
    fixed work or explicit non-goals from section 23.
13. Confirm that non-goals remain unimplemented unless added intentionally in a
    later spec revision.
14. Confirm that `spec.md` still matches repo layout, naming, and behavior.

## Tests

1. Existing pure-core and CLI parser tests pass unchanged on macOS.
2. macOS smoke test:
   - `memmy procs`
   - `memmy mods --pid <current-process-pid>` where permitted
   - safe current-process read where permitted
3. Permission-denied behavior is tested or manually verified with expected
   diagnostics.
4. Whole-repo build and test pass on supported platforms.

## Done When

- macOS either supports each backend operation or reports a clear supported
  status/error under platform constraints.
- A final audit shows every required initial feature from `spec.md` is
  implemented, tested, or explicitly covered by the spec's non-goals.
- The chunk sequence can be read from 1 through 10 as a complete path from the
  current repo state to the full specification.
