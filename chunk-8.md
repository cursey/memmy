# Chunk 8: Stable Output, JSON, And CLI Hardening

## Goal

Finish the CLI contract: stable JSON/JSONL output, standard error objects,
address formatting, help text, filtering behavior, capability checks, and exit
codes.

## Spec Coverage

- Section 5: error model, parser spans, JSON error object, exit codes.
- Section 15: full CLI option rules and command requirements.
- Section 17: address formatting, module-relative formatting, JSON rules.
- Section 22 milestone 6.
- Section 24: pleasant human output and stable agent output.

## Steps

1. Implement `Memmy_FormatAddress`.
2. Format 64-bit addresses as `0x0000000000000000` width and 32-bit addresses
   as `0x00000000` width.
3. Format module-relative expressions when the containing module can be safely
   represented as `<name>+0xoffset`; otherwise fall back to absolute addresses.
4. Add JSON escaping helpers for strings and bytes.
5. Implement successful JSON output for:
   - `procs`
   - `mods`
   - `addr`
   - `peek`
6. Implement JSONL successful output for `scan` and `pscan`.
7. Implement standard JSON error objects for every command when `--json` or
   `--jsonl` is active.
8. Implement text error formatting with source spans where available.
9. Centralize status-to-exit-code mapping.
10. Enforce global options:
    - `--json` applies to all commands
    - `--jsonl` applies only to `scan` and `pscan`
    - `--limit` applies to `scan` and `pscan`
    - `--chunk-size` applies to `scan` and `pscan`
    - unsupported or repeated single-use flags are CLI errors
11. Enforce command capability and process-access requirements exactly as
    listed in section 15.2.
12. Expand help text so it teaches address expressions, ranges, types, and
    examples without turning the CLI into a landing page.
13. Add ambiguous process-name handling if process-name selection is supported;
    otherwise ensure PID-only commands report invalid unsupported name options.

## Tests

1. Unit tests for fixed-width address formatting.
2. Unit tests for module-relative formatting and fallback cases.
3. Unit tests for JSON escaping, byte-array hex formatting, and stable keys.
4. CLI snapshot-style tests for text and JSON success output.
5. CLI snapshot-style tests for JSON and text errors.
6. Unit tests for exit-code mapping.
7. Unit tests for command capability checks and minimal access requests.

## Done When

- Every command has the specified text output and JSON or JSONL output.
- Failures consistently use `Memmy_Error` and the required exit codes.
- The CLI behavior is stable enough for humans and agents.
- The build and tests pass.
