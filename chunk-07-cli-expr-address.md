# Chunk 07: CLI --expr Address Resolution

## Goal

Wire top-level `memmy --expr` through `memmy_cli` for address-only expressions.

## Scope

- Add top-level `--expr <memory-expr>` option handling in `memmy_cli`.
- Reject `--expr` under subcommands such as `peek`, `poke`, `scan`, and
  `pscan`.
- For unqualified module or absolute expressions, require top-level `--pid` or
  `--name` when a process is needed.
- For process-qualified expressions, resolve the process in `memmy_cli`.
- Validate external `--pid`/`--name` conflicts with process-qualified
  expressions.
- Open the process with requirements derived by `memmy_exec`.
- List modules when required.
- Print resolved address results in text and JSON.

## Out of Scope

- Peek/poke expression execution.
- Scan expression execution.

## Tests

Add or extend `unittest/memmy/test_memmy_cli.c` and
`unittest/memmy/test_memmy_cli_expr.c`.

Required tests:

- `memmy --pid 1234 --expr "<client.dll>+0x123"` resolves and formats address.
- `memmy --pid 1234 --expr "<client.dll>+0x123->0x8"` resolves pointer chain.
- `memmy --expr "<game.exe!client.dll>+0x123"` resolves by process name.
- External `--pid` conflict with `<other!module>` fails.
- External `--name` conflict with `<other.exe!module>` fails.
- Subcommands reject `--expr`.
- JSON address-expression output matches spec.

## Completion Criteria

- Top-level address `--expr` works through `memmy_cli`.
- Existing v0 subcommands keep behavior.
- `ctest --test-dir build` passes.
