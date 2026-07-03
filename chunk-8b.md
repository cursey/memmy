# Chunk 8b: Stable Output, JSON, JSONL, And CLI Contract

## Goal

Finish the v0 CLI contract around stable text output, JSON/JSONL output,
standard error objects, global option validation, target selection, capability
checks, and exit codes.

This chunk assumes all command behavior exists in text mode.

## Spec Coverage

- `spec-v0.md` section 7: error model, JSON failures, and exit codes.
- `spec-v0.md` section 12: global CLI form, options, commands, target options,
  command requirements, and command output shapes.
- `spec-v0.md` section 13: output formatting and JSON rules.
- `spec-v0.md` section 15: JSON escaping and ambiguous process-name tests.
- `spec-v0.md` section 16 milestone 8.

## Steps

1. Centralize status-to-exit-code mapping:
   - success -> `0`
   - general failure -> `1`
   - CLI/parse/encoding errors -> `2`
   - not found or ambiguous -> `3`
   - access denied -> `4`
   - partial read/write -> `5`
   - unsupported -> `6`
   - platform error -> `7`
2. Implement stable status strings for JSON errors, such as `parse_error`.
3. Implement standard JSON failure output:
   - `ok`
   - `error.status`
   - `error.message`
   - `error.context`
   - `error.input`
   - `error.byte_offset`
   - `error.byte_count`
   - `error.os_code`
4. Implement JSON escaping helpers for strings and byte arrays.
5. Implement fixed-width lowercase address formatting:
   - 64-bit target: `0x0000000000000000`
   - 32-bit target: `0x00000000`
6. Implement successful JSON output for every command when `--json` is active.
   Use the exact object/array shapes specified by `spec-v0.md` where examples
   are given, and define stable command-shaped objects for the remaining
   command paths:
   - `procs`
   - `mods`
   - `regions`
   - `peek`
   - `poke`
   - `scan`
   - `pscan`
   For `scan` and `pscan`, `--json` returns one complete object:
   ```json
   {
     "results": [
       { "address": "0x00007ff800004242" },
       { "address": "0x00007ff800007abc" }
     ]
   }
   ```
7. Implement successful JSONL output for scan-result streaming:
   - `scan`
   - `pscan`
   Each JSONL record has the v0 scan result shape:
   ```json
   {"address":"0x00007ff800004242"}
   ```
8. Ensure `--json` applies to all commands.
9. Ensure `--jsonl` applies only to `scan` and `pscan`.
10. Validate repeated single-use flags and unsupported options as
    `Memmy_Status_InvalidArgument` with `context = "cli"`.
11. Implement or harden `--name <name>` target selection:
    - not found returns `Memmy_Status_NotFound`
    - multiple matches return `Memmy_Status_Ambiguous`
    - selection uses the process list rather than module names
12. Enforce command capability and process-access requirements exactly as listed
    in `spec-v0.md` section 12.
13. Ensure `poke` requires both `Read` and `Write` because v0 displays old/new
    values.
14. Ensure `scan` and `pscan` require `Read`, not `ListRegions`.
15. Keep help text focused on v0 syntax:
    - absolute `--addr`
    - `--start`/`--end`
    - `--start`/`--length`
    - supported types
    - pattern wildcards for `pscan`
16. Remove or reject any advertised full-spec syntax:
    - `addr`
    - `--expr`
    - `--range`
    - module-relative input
    - pointer chains
    - implicit whole-process scans

## Tests

1. Unit tests for exit-code mapping.
2. Unit tests for fixed-width address formatting.
3. Unit tests for JSON escaping, byte-array hex formatting, and stable keys.
4. CLI snapshot-style tests for text success output of every command.
5. CLI snapshot-style tests for JSON success output of every command.
6. CLI snapshot-style tests for JSONL success output of `scan` and `pscan`.
7. CLI snapshot-style tests for JSON and text failures.
8. CLI tests for global option validation and repeated single-use flags.
9. CLI tests for unsupported full-spec options and commands.
10. CLI tests for `--name` not found and ambiguous handling.
11. CLI tests for capability checks and requested process access.

## Done When

- Every v0 command has stable text and agent-facing output.
- Failures consistently use `Memmy_Error`, JSON error objects, and v0 exit
  codes.
- Help and validation expose only v0 syntax.
- The build and unit tests pass before starting Chunk 9.
