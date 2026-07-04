# Chunk 04: memmy_exec Foundation and Requirements

## Goal

Add the `memmy_exec` library and derive execution requirements from parsed
expressions.

## Scope

- Add `memmy_exec/` with:
  - `memmy_exec/CMakeLists.txt`
  - `memmy_exec/include/`
  - `memmy_exec/src/`
- Add `memmy_exec` after `memmy_expr` and before `memmy_cli` in top-level
  CMake.
- Link `memmy_exec` against `memmy_expr` and `memmy`.
- Define:

```c
typedef struct Memmy_ExecRequirements
{
    Memmy_BackendCap backend_caps;
    Memmy_ProcessAccess process_access;
    B32 needs_external_process;
    B32 needs_modules;
    B32 needs_regions;
} Memmy_ExecRequirements;
```

- Implement:

```c
Memmy_Status Memmy_MemoryExpr_GetRequirements(Memmy_MemoryExpr *expr,
                                              Memmy_ExecRequirements *out,
                                              Memmy_Error *error);
```

## Rules

- `memmy_exec` never enumerates or opens processes.
- `memmy_exec` never sets `ListProcs`.
- `needs_external_process` is true for absolute-address expressions or
  unqualified module targets without an expression process selector.
- `needs_modules` is true for module targets or module-relative ranges.
- `needs_regions` is true for whole-process ranges.
- `process_access` includes `Read` for peek, scans, and pointer-chain
  resolution.
- `process_access` includes `Write` for poke.

## Tests

Add `unittest/memmy/test_memmy_exec_requirements.c`.

Required tests:

- Absolute address resolution has no backend caps and no process access.
- Absolute peek requires `Read`.
- Absolute poke requires `Read | Write`.
- Module address requires `ListModules` and `Query`.
- Module peek/poke add module requirements to read/write requirements.
- Whole-process scans require `ListRegions` and `Query`.
- Parsed process-name selectors do not cause `memmy_exec` to set `ListProcs`.

## Completion Criteria

- `memmy_exec` builds and links.
- `memmy_test` links `memmy_exec`.
- `ctest --test-dir build` passes.
