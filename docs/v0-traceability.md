# Memmy v0 Traceability Checklist

This checklist audits `spec-v0.md` against the implementation at chunk 10.
Statuses use:

- `implemented and tested`: implemented in public API, CLI, backend, or tests.
- `v0 non-goal`: intentionally absent from v0.
- `missing and fixed`: a chunk-10 gap that was corrected during this audit.

## 1. Scope

- `implemented and tested`: CLI and C library inspect and modify local process
  memory through `procs`, `mods`, `regions`, `peek`, `poke`, `scan`, and
  `pscan`.
- `implemented and tested`: memory commands accept absolute remote addresses
  only.
- `implemented and tested`: scan commands require one explicit absolute range
  via `--start`/`--end` or `--start`/`--length`.
- `v0 non-goal`: expression evaluation and future Linux/macOS backends.

## 2. Explicit Non-Goals

- `implemented and tested`: CLI rejects `addr`, `--expr`, `--range`,
  `--readable`, `--writable`, and `--executable`.
- `implemented and tested`: numeric parsers reject module-relative,
  pointer-chain, parenthesized, signed, and arithmetic address input.
- `implemented and tested`: `scan` and `pscan` reject implicit whole-process
  invocation and duplicate range options.
- `v0 non-goal`: address expressions, range expressions, constant expressions,
  module-relative input, pointer chains, symbols, PDB/DWARF, repeated narrowing
  scans, remote machines, kernel memory, GUI, scripting, disassembly, remote
  allocation/free, memory protection changes, thread enumeration/suspension.

## 3. Project Shape

- `implemented and tested`: `base/`, `memmy/`, `cmd/memmy/`, `vendor/`, and
  `unittest/` match the v0 repository shape.
- `implemented and tested`: no `memmy_address_expr.*`, `cmd_addr.c`, or range
  expression parser exists.

## 4. Naming and Style

- `implemented and tested`: public API uses `Memmy_` names, `Memmy_Addr` and
  `Memmy_Size` are `U64`, and remote addresses are integers outside the Win32
  backend boundary.

## 5. Address and Size Input

- `implemented and tested`: `Memmy_ParseAddress` and `Memmy_ParseSize` accept
  decimal and `0x`/`0X` hex unsigned integer tokens.
- `implemented and tested`: invalid syntax returns `Memmy_Status_ParseError`;
  overflow returns `Memmy_Status_Overflow`.

## 6. Ranges

- `implemented and tested`: `Memmy_Range`, `Memmy_Range_FromStartEnd`, and
  `Memmy_Range_FromStartLength` implement half-open `[start, end)` ranges.
- `implemented and tested`: start/end ordering, start/length overflow, and
  zero-length ranges are covered.

## 7. Error Model

- `implemented and tested`: required statuses, `Memmy_Error`, JSON failure
  shape, stable status strings, and exit-code mapping are implemented.
- `implemented and tested`: initial contexts include address, range, type,
  value, pattern, backend, and cli.

## 8. Backend Boundary

- `implemented and tested`: `Memmy_Context`, `Memmy_Backend`, and capability
  bits match v0.
- `implemented and tested`: platform SDK includes and native process/memory
  calls are isolated under `memmy/src/platform/win32/`.

## 9. Process, Module, and Region Types

- `implemented and tested`: process, module, region, access, state, and pointer
  width types are declared through `memmy.h`.
- `implemented and tested`: required APIs for process listing, open/close,
  module/region listing, read, and write are declared, defined, and dispatched.

## 10. Types, Values, and Patterns

- `implemented and tested`: required scalar, pointer, bytes, `str`, and `wstr`
  type names parse.
- `implemented and tested`: `Memmy_Value_Parse` handles pointer width and
  value encoding.
- `implemented and tested`: pattern parsing accepts wildcards only when
  `Memmy_PatternParseFlag_AllowWildcards` is set; bytes values reject
  wildcards.

## 11. Scanning

- `implemented and tested`: `Memmy_Process_ScanValue` and
  `Memmy_Process_ScanPattern` operate on one caller-provided range.
- `implemented and tested`: reads stay in range, matches may cross chunk
  boundaries, limits cap result count, addresses are absolute, zero-length
  ranges produce no results, and scans work with and without `ListRegions`.
- `implemented and tested`: region enumeration is only an optimization and does
  not create implicit input ranges.

## 12. CLI

- `implemented and tested`: global form and options are present.
- `implemented and tested`: required commands exist: `procs`, `mods`,
  `regions`, `peek`, `poke`, `scan`, and `pscan`.
- `implemented and tested`: `--pid` and `--name` target selection work; name
  ambiguity returns `Memmy_Status_Ambiguous`.
- `implemented and tested`: backend capability and process access requirements
  match v0.
- `implemented and tested`: each command rejects unsupported v0 syntax and
  invalid option combinations with clear CLI errors.

## 13. Output Formatting

- `implemented and tested`: addresses format as fixed-width lowercase hex for
  32-bit and 64-bit targets.
- `implemented and tested`: JSON success output is command-shaped; failures use
  the standard error object; JSONL is available for `scan` and `pscan`.
- `implemented and tested`: byte arrays use lowercase two-digit hex bytes
  separated by one space.

## 14. Windows Backend

- `implemented and tested`: Win32 backend implements process enumeration,
  opening, read/write, module listing, and region listing.
- `implemented and tested`: same-bitness current-process read/write/inventory
  and scan smoke tests run in the unit suite on Windows.
- `implemented and documented`: WOW64 and protected-process checks remain
  manual/platform-dependent in `docs/windows-smoke-tests.md`.

## 15. Testing

- `implemented and tested`: unit coverage exists for address/size/range
  parsing, region listing, value parsing, pattern parsing, wildcard rejection,
  peek, poke, scans with one range, scans with and without `ListRegions`,
  chunk-boundary matches, JSON escaping, and ambiguous process names.
- `missing and fixed`: chunk 10 added an audit-specific negative CLI test for
  v0 non-goal syntax.

## 16. Milestones

- `implemented and tested`: milestones 1 through 8 are represented by the
  chunk sequence and covered by unit/CLI tests.
