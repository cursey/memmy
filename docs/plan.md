# Plan: Add `memmy_ast` and `memmy_eval` for the Pocket-Reference DSL

## Summary

Build the new DSL as parallel libraries, leaving `memmy_dsl` and `memmy_exec` untouched until the new path is mature. `memmy_ast` owns tokenization, parsing, AST nodes, and diagnostics. `memmy_eval` owns runtime values, variables, process/module resolution, reads/writes, scans, indexing, and REPL commands. The CLI switches only after parser and evaluator parity are independently tested.

Chosen defaults: new syntax only, parallel-first migration, small reviewable PRs.

## Public Interfaces

- Add `memmy_ast/include/memmy_ast.h` with:
  - `Memmy_AstNode`, `Memmy_AstNodeKind`, `Memmy_AstStatement`, `Memmy_AstCommandKind`.
  - Parse APIs: `Memmy_Ast_ParseExpr`, `Memmy_Ast_ParseStatement`.
  - Node kinds for const arithmetic, variables, targets, addresses, ranges, deref, typed read/write, pattern scan, value scan, index, assignment, and command.
- Add `memmy_eval/include/memmy_eval.h` with:
  - `Memmy_EvalEnv`, `Memmy_EvalValue`, `Memmy_EvalResult`, `Memmy_EvalResultSink`.
  - Value kinds: `Const`, `Address`, `Range`, `AddressList`, `TypedValue`, `Null`.
  - APIs: `Memmy_EvalEnv_Create`, `Memmy_EvalStatement`, `Memmy_EvalExpr`, `Memmy_EvalEnv_Set/Find/Unset/Clear`.
- Add CMake targets:
  - `memmy_ast` links `base` only and stores types, patterns, and values as syntax/raw text until evaluation.
  - `memmy_eval` links `memmy_ast` and `memmy`.
  - Tests link both new libraries while old tests keep linking old libraries.

## Phases

### Phase 1: Library Scaffolding

- Add empty `memmy_ast` and `memmy_eval` targets, public headers, source folders, and test suites.
- Do not modify CLI behavior.
- Tests: build target compiles; test runner discovers empty/new smoke suites.

### Phase 2: Lexer and Core AST Parser

- Implement a tokenizer in `memmy_ast` for identifiers, variables, integers, strings, targets, pattern braces, brackets, `@`, `->`, `..`, `+`, `-`, `*`, `/`, `%`, `=`, `==`, `as`, commands, and parentheses.
- Parse constants, variable refs, target refs, and assignments.
- Parse `$foo = 42` as const assignment, not address assignment.
- Tests: constants with precedence, variables, invalid identifiers, targets from the reference, assignment basics, precise diagnostic offsets.

### Phase 3: Address and Range Parsing

- Parse absolute addresses as `@const_expr`.
- Parse target address bases like `<client.dll>+0x1234`.
- Parse deref chains: `->`, `->offset`, `->-offset`, `->(expr)`, `->$var`.
- Parse ranges: `[@a..@b]`, `[@a..+n]`, `<target>`.
- Reject old spellings in `memmy_ast`: bare numeric address, `address:+size`, and `<pid!>0x1234`.
- Tests: every Core Values, Targets, Ranges, and Addresses example in the pocket reference.

### Phase 4: Reads, Writes, Scans, Indexing

- Parse typed reads: `address as T`.
- Parse typed writes: `address as T = value_text`.
- Parse pattern scans: `range{pattern}`.
- Parse value scans: `range as T == value_text`.
- Parse indexing for any expression that yields an address list: `expr[N]`, including parenthesized scans and variables.
- Tests: Reads, Writes, Address Lists, Indexing Address Lists, and assignment examples from the pocket reference.

### Phase 5: Eval Core Values

- Implement `Memmy_EvalValue` ownership in arenas.
- Evaluate constants, variables, assignments, address arithmetic, ranges, and module/process target resolution.
- Variables bind evaluated values immediately.
- Define arithmetic rules:
  - `Const +/- Const -> Const`
  - `Address +/- Const -> Address`
  - integer `TypedValue` is accepted anywhere `Const` is expected, preserving typed display metadata while keeping arithmetic ergonomic
  - deref requires a selected/open process
  - unsupported type combinations return `Memmy_Status_InvalidArgument`.
- Tests: const variables, address variables, range variables, cycles, wrong-kind variables, overflow, missing process diagnostics.

### Phase 6: Reads, Writes, Scans, and Address Lists

- Evaluate typed reads into `TypedValue`; integer `TypedValue`s are accepted anywhere `Const` is expected, preserving typed display metadata while keeping arithmetic ergonomic.
- Evaluate writes by resolving address, reading old value, writing new value, and emitting a result; typed writes require read and write access because the old value is part of the result.
- Evaluate pattern/value scans into materialized `AddressList` values.
- Evaluate indexing into `AddressList` as `Address`; out-of-range is `Memmy_Status_NotFound`.
- Tests: fake backend read/write, pattern scan assignment, value scan assignment, `$matches[N]`, `(<range> as T == value)[N]`, example flow `$anchor/$target`.

### Phase 7: Statement Evaluation and Commands

- Implement statement-level evaluation in `memmy_eval`.
- Add commands: `/procs [filter]`, `/mods [filter]`, `/regions`, `/vars`, `/unset $var`, `/clear`, `/help`, `/exit`, `/quit`.
- Emit structured eval results for values, reads, writes, address lists, process/module/region/variable listings, help, and exit. Formatting remains a CLI responsibility.
- Tests: command parsing/eval, fuzzy filters, `/vars`, `/unset $var`, `/clear`, help, and exit control results.

### Phase 8: CLI Integration

- Add CLI formatter support for `Memmy_EvalResult`.
- Switch REPL and `--expr` to `memmy_ast + memmy_eval`.
- Keep old `memmy_dsl + memmy_exec` compiled and tested during the first CLI switch PR.
- Update help text to match `docs/memmy-dsl-pocket-reference.md`.
- Tests: CLI/repl golden outputs for new syntax, structured output for new result kinds if structured output is still retained, attached-process behavior, old syntax rejection.

### Phase 9: Retirement

- After the new CLI path passes all existing equivalent scenarios plus pocket-reference tests, remove `memmy_dsl` and `memmy_exec`.
- Remove old tests or port useful coverage to `memmy_ast`/`memmy_eval`.
- Update CMake dependencies and public includes so `memmy_cli` no longer exposes `memmy_exec.h`.
- Tests: full configure/build/test from clean tree.

## Test Plan

- Every valid example in `docs/memmy-dsl-pocket-reference.md` must appear in parser tests; old/invalid spellings must appear in rejection tests.
- Evaluator tests use the existing fake backend where possible, with focused tests for reads, writes, scans, modules, regions, and pointer deref.
- CLI tests verify text formatting and any retained structured-output formatting after integration.
- Each phase must pass `cmake --build build` and the relevant `memmy_test` cases before proceeding.

## Assumptions

- `memmy_ast` implements only the new pocket-reference syntax; old syntax remains available only through `memmy_dsl` until retirement.
- `memmy_ast` is parser-pure and does not depend on `memmy`; semantic type, pattern, and value validation happens in `memmy_eval`.
- The new evaluator materializes scan results for assignment and indexing.
- Integer typed reads can participate in constant arithmetic while retaining type metadata for display.
- Typed writes require read and write access so the evaluator can report old and new values.
- Whole-process targets remain valid scan ranges; module targets resolve to module ranges.
- String and byte value parsing continues to reuse existing `Memmy_Value_Parse` and `Memmy_Pattern_Parse`.
