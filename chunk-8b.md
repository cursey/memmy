# Chunk 8b: CLI Hardening, Help, And Capability Contract

## Goal

Finish the CLI behavior contract around global option validation, command
capability checks, process access requirements, filtering behavior, and help
text.

This chunk assumes Chunk 8 has already centralized output rendering, error
formatting, and exit-code mapping.

## Spec Coverage

- Section 15: full CLI option rules and command requirements.
- Section 22 milestone 6.
- Section 24: pleasant human output and stable agent output.

## Steps

1. Enforce global options:
   - `--json` applies to all commands
   - `--jsonl` applies only to `scan` and `pscan`
   - `--limit` applies to `scan` and `pscan`
   - `--chunk-size` applies to `scan` and `pscan`
   - unsupported or repeated single-use flags are CLI errors
2. Harden filtering behavior for commands that support `--filter`, including
   empty filters, case-sensitivity rules from the spec, and stable no-match
   output.
3. Enforce command capability and process-access requirements exactly as listed
   in section 15.2.
4. Expand help text so it teaches address expressions, ranges, types, and
   examples without turning the CLI into a landing page.
5. Add ambiguous process-name handling if process-name selection is supported;
   otherwise ensure PID-only commands report invalid unsupported name options.

## Tests

1. Unit tests for global option validation and repeated single-use flags.
2. Unit tests for filter parsing, empty filters, no-match output, and
   case-sensitivity behavior.
3. Unit tests for command capability checks and minimal access requests.
4. CLI snapshot-style tests for expanded help text.
5. CLI tests for ambiguous process-name handling if process-name selection is
   supported.
6. CLI tests proving unsupported process-name options fail clearly when commands
   are PID-only.

## Done When

- Global options are accepted or rejected according to the spec.
- Filter behavior is stable for supported commands, including empty and no-match
  cases.
- Commands check backend capabilities and request the required minimal process
  access before doing work.
- Help output is complete enough for humans without destabilizing agent-facing
  output.
- The build and tests pass.
