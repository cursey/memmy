# Memmy v1 Agent Loop

Use this file in a new Codex session to run the v1 chunk plan. The goal is to
execute `chunk-01-*.md` through `chunk-11-*.md` sequentially until the repo
satisfies `spec-v1.md`.

## Operating Rules

- There are three roles:
  - **Orchestrator**: owns sequencing, context, final approval, and `jj`
    boundaries.
  - **Worker**: implements exactly one chunk.
  - **Reviewer**: reviews exactly one chunk.
- Use one Worker and one Reviewer per chunk. Do not reuse Worker or Reviewer
  agents across chunks.
- Within a chunk, reuse the same Worker and the same Reviewer for every
  iteration until that chunk is approved. Do not spawn replacement Worker or
  Reviewer agents mid-chunk unless the session is genuinely lost or blocked.
- Execute chunks one after another. Do not parallelize chunks.
- A chunk is complete only when Worker, Reviewer, and Orchestrator all approve.
- Worker and Reviewer must iterate until Reviewer has no blocking findings.
- Orchestrator must independently inspect the final diff and test outcome before
  approving.
- After a chunk is complete:
  - Orchestrator runs `jj describe` for the completed change.
  - Orchestrator runs `jj new` to create the next change.
  - Orchestrator spawns a fresh Worker and fresh Reviewer for the next chunk.
- Preserve existing user changes. Do not revert unrelated work.
- Keep edits scoped to the active chunk.

## Chunk Order

```txt
chunk-01-memmy-expr-foundation.md
chunk-02-memmy-expr-address.md
chunk-03-memmy-expr-top-level.md
chunk-04-memmy-exec-requirements.md
chunk-05-memmy-exec-address-resolution.md
chunk-06-memmy-exec-pointer-chains.md
chunk-07-cli-expr-address.md
chunk-08-memmy-exec-peek-poke.md
chunk-09-ranges-pattern-scan.md
chunk-10-whole-process-value-scan.md
chunk-11-output-errors-help.md
```

## Orchestrator Prompt

```txt
You are the Orchestrator for the Memmy v1 implementation loop.

Read:
- AGENTS.md
- spec-v1.md
- all chunk-*.md files

Run the chunks sequentially. For each chunk:

1. Confirm the working copy state with `jj status` and `git status --short`.
2. Read the active chunk file.
3. Spawn one fresh Worker agent for only that chunk.
4. When Worker reports completion, spawn one fresh Reviewer agent for only that
   chunk.
5. If Reviewer has blocking findings, send them back to the same Worker for
   that chunk.
6. Repeat with the same Worker and same Reviewer until Reviewer approves.
7. Independently inspect:
   - active chunk requirements
   - current diff
   - tests run and results
   - dependency boundaries
   - whether unrelated changes were avoided
8. If Orchestrator finds issues, send them back to Worker and then Reviewer.
9. When Worker, Reviewer, and Orchestrator approve:
   - run `jj status`
   - run `jj diff --stat`
   - run `jj describe -m "<chunk number>: <short chunk title>"`
   - run `jj new`
10. Continue with the next chunk using a new Worker and new Reviewer.

Do not run chunks in parallel. Reuse Worker and Reviewer within a chunk; do not
reuse them between chunks.

The final state after chunk 11 must satisfy spec-v1.md and all chunk completion
criteria.
```

## Worker Prompt Template

Use this template once per chunk with a fresh Worker.

```txt
You are the Worker for exactly one Memmy v1 chunk.

Active chunk:
<chunk filename>

Read:
- AGENTS.md
- spec-v1.md
- <chunk filename>
- relevant existing source and tests

Your job:
- Implement only the active chunk.
- Keep edits scoped to the chunk.
- Preserve existing user changes.
- Add or update tests required by the chunk.
- Run the narrowest useful tests first, then broader tests required by the
  chunk completion criteria.
- Report:
  - files changed
  - behavior implemented
  - tests run
  - known limitations or follow-up items that remain outside the chunk scope

Do not start work from any later chunk. Do not change unrelated code.
Stop only when the active chunk is implemented and tested, or when genuinely
blocked.
```

## Reviewer Prompt Template

Use this template once per chunk with a fresh Reviewer after Worker completes.

```txt
You are the Reviewer for exactly one Memmy v1 chunk.

Active chunk:
<chunk filename>

Read:
- AGENTS.md
- spec-v1.md
- <chunk filename>
- the Worker summary
- the current diff
- relevant source and tests

Review stance:
- Prioritize bugs, regressions, spec mismatches, missing tests, dependency
  boundary violations, and unintended scope creep.
- Findings must be concrete and actionable.
- Include file/line references.
- Classify findings as blocking or non-blocking.
- Verify that the Worker did not implement later chunks.
- Verify that tests match the chunk completion criteria.
- Verify that existing v0 behavior remains intact.

If there are blocking findings, do not approve. Send the findings back to the
Worker.

Approve only when the active chunk satisfies:
- its Scope
- its Tests
- its Completion Criteria
- relevant parts of spec-v1.md
```

## Per-Chunk Approval Checklist

Orchestrator should complete this checklist before `jj describe`.

```txt
Chunk:

Worker approval:
[ ] Worker says implementation is complete.
[ ] Worker listed files changed.
[ ] Worker listed tests run.

Reviewer approval:
[ ] Reviewer has no blocking findings.
[ ] Reviewer confirmed scope boundaries.
[ ] Reviewer confirmed tests.

Orchestrator approval:
[ ] I read the active chunk.
[ ] I inspected the final diff.
[ ] I verified relevant tests passed.
[ ] I verified no unrelated changes were made.
[ ] I verified dependency boundaries.
[ ] I verified this chunk does not implement later chunks unnecessarily.

Handoff:
[ ] jj status reviewed.
[ ] jj diff --stat reviewed.
[ ] jj describe -m "<chunk number>: <short chunk title>" completed.
[ ] jj new completed.
```

## Suggested `jj describe` Messages

```txt
01: add memmy_expr foundation
02: parse address expressions
03: parse top-level memory expressions
04: add memmy_exec requirements
05: resolve address expressions
06: resolve pointer chains
07: wire expr address CLI
08: execute expr peek and poke
09: execute expr pattern scans
10: execute expr value scans
11: harden expr output and errors
```

## Final Completion Checklist

After chunk 11:

```txt
[ ] All chunk files completed in order.
[ ] Each chunk has its own jj change.
[ ] All tests pass with `ctest --test-dir build`.
[ ] Required acceptance examples from spec-v1.md work.
[ ] `spec-v1.md` architecture boundaries hold:
    [ ] memmy_expr parses only.
    [ ] memmy_exec resolves/executes only.
    [ ] memmy_cli owns argv/process-selection/output policy.
    [ ] cmd_memmy remains thin.
[ ] No v1 non-goals were implemented accidentally.
```
