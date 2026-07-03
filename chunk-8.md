# Chunk 8: Stable Output, JSON, And Error Contract

## Goal

Finish the CLI output contract: stable JSON/JSONL output, standard error
objects, address formatting, text error spans, and exit codes.

## Spec Coverage

- Section 5: error model, parser spans, JSON error object, exit codes.
- Sections 15.3-15.9: command text output and JSON/JSONL output shapes.
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
6. Upgrade `scan` and `pscan` text output from absolute-only addresses to
   include module-relative display when available.
7. Implement JSONL successful output for `scan` and `pscan`.
8. Implement standard JSON error objects for every command when `--json` or
   `--jsonl` is active.
9. Implement text error formatting with source spans where available.
10. Centralize status-to-exit-code mapping.

## Tests

1. Unit tests for fixed-width address formatting.
2. Unit tests for module-relative formatting and fallback cases.
3. CLI snapshot-style tests proving `scan` and `pscan` text output include
   module-relative display when available and fall back to absolute-only
   addresses when not.
4. Unit tests for JSON escaping, byte-array hex formatting, and stable keys.
5. CLI snapshot-style tests for text and JSON success output.
6. CLI snapshot-style tests for JSON and text errors.
7. Unit tests for exit-code mapping.

## Done When

- Every command has the specified text output and JSON or JSONL rendering for
  already-supported command paths.
- Failures consistently use `Memmy_Error` and the required exit codes.
- The output behavior is stable enough for humans and agents.
- The build and tests pass.
