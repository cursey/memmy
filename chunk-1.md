# Chunk 1: Public API Foundation

## Goal

Turn the current placeholder `memmy` interface library into a real C11 core
library with the public types, source layout, status model, context model, and
backend abstraction required by `spec.md`.

## Spec Coverage

- Sections 2-6: naming, source layout, core concepts, error model, platform backend boundary.
- Section 21: test backend shape needed by later pure-core tests.
- Section 24: API shape is not Windows-only; remote addresses are integers.

## Steps

1. Replace the current `memmy` INTERFACE target with a compiled library target.
2. Create the header layout described in `spec.md` under `memmy/include/`.
3. Create the matching implementation layout under `memmy/src/`, including `platform/win32/` placeholders where needed.
4. Define foundational public types:
   - `Memmy_Addr`, `Memmy_Size`
   - `Memmy_Status`, `Memmy_Error`
   - `Memmy_Context`
   - `Memmy_Backend`
   - `Memmy_Process`, `Memmy_PointerWidth`, `Memmy_Endian`
5. Implement thread-local context functions:
   - `Memmy_Context_Get`
   - `Memmy_Context_Set`
   - `Memmy_Context_Push`
   - `Memmy_Context_Pop`
   - `Memmy_Context_InitDefault`
6. Add status-to-string helpers if needed by tests and CLI output, keeping names stable and `Memmy_` prefixed.
7. Add small error helper routines for consistent initialization of `Memmy_Error` without introducing global error state.
8. Add `test/test_memmy_backend.h` and `test/test_memmy_backend.c` in the repo's current `unittest/` structure or rename the test directory only if the whole build is updated consistently.
9. Implement the test backend enough to expose:
   - one fake process id
   - fixed memory buffer
   - fake module list
   - fake region list
   - configurable pointer width
   - read/write callbacks
10. Wire the new library and tests into CMake.

## Tests

1. Build succeeds with `cmake --build build`.
2. Unit tests prove that `memmy.h` exports all foundational headers.
3. Unit tests prove context push/pop is thread-local from the caller's perspective.
4. Unit tests prove platform-touching APIs reject a missing current context or missing backend with `Memmy_Status_InvalidArgument`.
5. Unit tests prove `Test_MemmyBackend_AsBackend` returns a usable `Memmy_Backend`.

## Done When

- No production code outside `memmy/src/platform/<os>/` includes OS SDK headers.
- `spec.md` remains unchanged.
- The repo builds with the new compiled `memmy` target.
- Later chunks can depend on `Memmy_Backend`, `Memmy_Context`, and the test backend.
