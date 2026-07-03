# Chunk 1: v0 Public API Foundation

## Goal

Turn the current placeholder `memmy` interface into a real C11 core library
with the foundational public API required by `spec-v0.md`.

This chunk establishes the type, error, context, backend, process, module, and
region shapes that every later chunk depends on. It deliberately does not add
address-expression, range-expression, Linux, or macOS work.

## Spec Coverage

- `spec-v0.md` sections 3-4: project shape, naming, remote address integer
  types.
- `spec-v0.md` section 7: status and error model.
- `spec-v0.md` section 8: backend boundary, context, backend callbacks, backend
  capabilities.
- `spec-v0.md` section 9: process, module, and region type definitions.
- `spec-v0.md` section 15: test backend shape needed by later tests.

## Steps

1. Replace the current `memmy` INTERFACE target with a compiled C11 library
   target linked against `base`.
2. Create the v0 header layout under `memmy/include/`, keeping public project
   names `Memmy_` prefixed.
3. Create matching implementation files under `memmy/src/`, with native SDK
   includes reserved for `memmy/src/platform/<os>/`.
4. Define foundational public types:
   - `Memmy_Addr`
   - `Memmy_Size`
   - `Memmy_Status`
   - `Memmy_Error`
   - `Memmy_Context`
   - `Memmy_Backend`
   - `Memmy_BackendCap`
   - `Memmy_PointerWidth`
   - `Memmy_ProcessAccess`
   - `Memmy_Process`
   - `Memmy_ProcessInfo`
   - `Memmy_ProcessList`
   - `Memmy_Module`
   - `Memmy_ModuleList`
   - `Memmy_RegionAccess`
   - `Memmy_RegionState`
   - `Memmy_Region`
   - `Memmy_RegionList`
5. Implement thread-local context helpers if the codebase does not already have
   them:
   - `Memmy_Context_Get`
   - `Memmy_Context_Set`
   - `Memmy_Context_Push`
   - `Memmy_Context_Pop`
   - `Memmy_Context_InitDefault`
6. Implement status and error helpers needed by tests and CLI output, such as
   stable status names and a small helper to fill `Memmy_Error` consistently.
7. Add portable backend-dispatch stubs for the public APIs from section 9:
   missing context, missing backend, missing callback, or unsupported
   capability must return a precise non-OK status instead of crashing.
8. Add the test backend under the existing `unittest/` structure unless the
   whole build is intentionally renamed. The backend must expose:
   - fixed fake process metadata
   - configurable pointer width
   - fake module list
   - fake region list
   - fixed memory buffer
   - read/write callbacks
9. Wire the new library and test backend into CMake.
10. Keep `spec-v0.md` as the target reference and do not add files that v0
    excludes, especially `memmy_address_expr.*` or a range-expression parser.

## Tests

1. Configure and build:
   - `cmake -S . -B build`
   - `cmake --build build`
2. Unit tests prove `memmy.h` includes the v0 public headers and exposes the
   foundational types.
3. Unit tests prove context set/push/pop restores the caller's context.
4. Unit tests prove backend-dispatch APIs reject missing context, missing
   backend, and missing callbacks with `Memmy_Status_InvalidArgument` or
   `Memmy_Status_Unsupported` as appropriate.
5. Unit tests prove `Test_MemmyBackend_AsBackend` returns a usable
   `Memmy_Backend` with accurate capabilities.

## Done When

- The `memmy` target is a compiled library.
- No production code outside `memmy/src/platform/<os>/` includes native OS SDK
  headers.
- Remote process addresses are represented as `Memmy_Addr` outside backend
  boundary code.
- The build and unit tests pass before starting Chunk 2.
