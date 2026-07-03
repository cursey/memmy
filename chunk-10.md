# Chunk 10: Final v0 Audit And Spec Alignment

## Goal

Perform a requirement-by-requirement audit proving the implementation satisfies
`spec-v0.md`, and update any repo documentation that still points users or
agents at full-spec behavior for v0 work.

This is not a feature-expansion chunk. It is the final correctness pass for the
v0 target.

## Spec Coverage

- All of `spec-v0.md`.
- `spec-v0.md` section 2: explicit non-goals.
- `spec-v0.md` section 15: important v0 tests.
- `spec-v0.md` section 16: milestone completion.

## Steps

1. Build a traceability checklist from every `spec-v0.md` section and mark each
   requirement as:
   - implemented and tested
   - intentionally unsupported because it is a v0 non-goal
   - missing and fixed in this chunk before completion
2. Confirm these full-spec features are absent or explicitly rejected:
   - address expression parsing or resolution
   - range expression parsing or resolution
   - constant-expression parsing or evaluation
   - module-relative input syntax
   - pointer-chain resolution
   - implicit whole-process scans
   - multiple input addresses or ranges per invocation
   - symbol loading
   - repeated narrowing scans
   - remote allocation/free
   - memory protection changes
   - thread enumeration or suspension
   - Linux/macOS backend implementation as part of v0
3. Confirm every required public API from `spec-v0.md` is declared, defined,
   named correctly, and included through `memmy.h`.
4. Confirm every required CLI command exists:
   - `procs`
   - `mods`
   - `regions`
   - `peek`
   - `poke`
   - `scan`
   - `pscan`
5. Confirm every command rejects unsupported v0 syntax with clear CLI errors.
6. Confirm `spec-v0.md` examples match implemented option names and output
   shapes.
7. Confirm the broader future design document remains separate from this v0
   chunk sequence, which clearly targets `spec-v0.md`.
8. Update nearby planning docs only if they still instruct implementers to build
   full-spec behavior for the v0 chunk sequence.
9. Run formatting on changed C sources if this audit includes code fixes.
10. Run the complete build and test suite.
11. Record any intentionally skipped Windows integration checks with concrete
    reasons, such as permission limits or unavailable WOW64 environment.

## Tests

1. `cmake -S . -B build`
2. `cmake --build build`
3. Complete unit test executable or `ctest` invocation used by the repo.
4. CLI snapshot/integration tests for every command in text mode.
5. CLI snapshot/integration tests for required JSON/JSONL success output.
6. CLI snapshot/integration tests for JSON and text error output.
7. Windows smoke tests from Chunk 9.
8. Audit-specific negative tests proving non-goal syntax is not accidentally
   accepted.

## Done When

- The chunk sequence from Chunk 1 through Chunk 10 is a complete path to
  satisfying `spec-v0.md`.
- All chunks use `spec-v0.md` as their implementation target.
- Every v0 requirement is implemented and tested, or explicitly identified as a
  v0 non-goal.
- The final build and test suite pass.
