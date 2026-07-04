# Chunk Agent Loop

Use this runbook to continue the remaining `chunk-*.md` implementation work in a hands-off, repeatable way.

## Roles

- **Orchestrator**: the top-level Codex session operating in the real worktree.
- **Worker**: exactly one sub-agent per chunk, responsible for implementing that chunk.
- **Reviewer**: exactly one sub-agent per chunk, responsible for reviewing that chunk. Reuse the same reviewer for all re-reviews of that chunk.

Do not create multiple workers or multiple reviewers for the same chunk unless the user explicitly changes this process.

## Orchestrator Loop

For each remaining chunk, in order:

1. Confirm the current `jj` state:
   ```powershell
   jj status
   jj log -r "@-::@" --no-graph
   ```
2. Read the next chunk file, for example:
   ```powershell
   Get-Content chunk-7.md
   ```
3. Spawn one worker agent for that chunk.
   - Tell the worker they are not alone in the codebase.
   - Tell the worker not to revert unrelated/user changes.
   - Give the worker ownership of implementing the chunk.
   - Ask the worker to edit files directly, run relevant tests when practical, and report changed paths plus verification.
4. Integrate/review the worker output in the orchestrator worktree.
   - If the worker changed files in its forked workspace, inspect and apply the relevant patch locally.
   - Run formatting as needed.
   - Run at least:
     ```powershell
     cmake --build build
     build\unittest\Debug\memmy_test.exe
     ```
5. Spawn one reviewer agent for that chunk.
   - The reviewer must review only the diff for that chunk against its parent chunk.
   - The reviewer must not edit files.
   - Ask for findings first, with severity and file/line references.
6. Decide whether each reviewer finding has merit.
   - If a finding is valid, fix it through the same worker agent where practical, or apply a tightly scoped orchestrator integration fix if faster/safer.
   - If a finding is invalid or intentionally out of scope, document the reason and ask the same reviewer whether they are satisfied with that disposition.
7. Re-review loop:
   - Ask the same reviewer agent if they are satisfied after each accepted fix or rejected finding.
   - Reuse the same worker agent for fix iterations.
   - Repeat until the worker reports done and the reviewer reports no blocking findings / satisfied.
8. Final verification:
   ```powershell
   clang-format -i <touched-c-files-and-headers>
   cmake --build build
   build\unittest\Debug\memmy_test.exe
   jj diff --stat
   ```
9. Finalize the chunk:
   ```powershell
   jj describe -m "Implement chunk-N.md"
   jj new
   ```
10. Begin the next chunk with a fresh worker/reviewer pair.

## Worker Prompt Template

```text
You are the worker agent for chunk-N in D:\repos\memmy.

You are not alone in the codebase. Do not revert unrelated changes or user changes. Work with the current state.

Read chunk-N.md and implement its requirements. Edit files directly in your workspace. Keep the implementation scoped to this chunk and consistent with AGENTS.md. Run relevant formatting/build/tests if practical.

When done, report:
- changed files
- what was implemented
- tests/build commands run and results
- known gaps or risks
```

## Reviewer Prompt Template

```text
You are the reviewer agent for chunk-N in D:\repos\memmy.

Review only the chunk-N diff against its parent chunk. Do not edit files.

Focus on:
- bugs
- missed chunk-N requirements
- behavioral regressions
- meaningful test gaps

Return findings first, ordered by severity, with file/line references. If there are no findings, say "Findings: none" and note residual risk. You may run tests if useful.
```

## Reviewer Recheck Prompt Template

```text
Please re-check chunk-N after the latest fixes/dispositions.

Use the same review scope as before. Specifically verify the prior findings are fixed or acceptably dispositioned. Do not edit files.

Return whether you are satisfied. If not, list remaining findings with severity and file/line references.
```

## Current Handoff State

As of the last session:

- `chunk-1.md` through `chunk-6.md` are complete.
- `chunk-6.md` was reviewed by a separate reviewer and accepted after fixes.
- Current active chunk is `chunk-7.md` / **Explicit-Range Pattern Scan**.
- Current working copy is a child change on top of `Implement chunk-6.md`.
- Chunk 7 is partially implemented, not complete, not tested, and not reviewed.

Current partial chunk-7 files:

- `cmd/memmy/memmy_cli.c`
- `memmy/include/memmy.h`
- `memmy/include/memmy_scan.h`
- `memmy/src/memmy_scan.c`

Current partial chunk-7 work includes:

- Added `Memmy_ScanOptions`, `Memmy_ScanResult`, `Memmy_ScanResultList`.
- Added `Memmy_Process_ScanPattern`.
- Added chunked scan logic with overlap and optional readable-region intersection.
- Started CLI parsing for `pscan` options:
  - `--start`
  - `--end`
  - `--length`
  - `--limit`
  - `--chunk-size`
  - `--pattern`
- Started command validation so pscan-only options are rejected by other commands.

Remaining chunk-7 work:

- Finish `memmy pscan` command execution and output formatting.
- Add scan tests required by `chunk-7.md`.
- Build and run tests.
- Run the worker/reviewer loop for chunk 7.
- When accepted, run:
  ```powershell
  jj describe -m "Implement chunk-7.md"
  jj new
  ```

