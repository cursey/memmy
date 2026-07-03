# Proposed Spec Changes

This document captures experimental language and CLI ideas that are not yet
accepted into `spec.md`.

The current specification uses subcommands as the primary interface:

```txt
memmy peek --pid 1234 --expr '<client.dll>+0x4242' --type u32
memmy poke --pid 1234 --expr '<client.dll>+0x4242' --type u32 --value 1337
memmy scan --pid 1234 --range '<client.dll>' --type u32 --value 1337
memmy pscan --pid 1234 --range '<client.dll>' --pattern '48 8B ?? ?? 89'
```

The proposed direction is an expression-oriented front end:

```txt
memmy
memmy --expr "<game.exe!client.dll>+0x123 : i32"
memmy --expr "<game.exe!client.dll>+0x123 : i32 = 77"
```

Invoking `memmy` with no arguments would start a REPL. `memmy --expr` would be
the shell-safe automation and agent entry point.

The existing subcommands may remain as canonical explicit forms for scripts.

---

## Goals

The expression language should support common memory inspection workflows with
compact, copy-pasteable expressions:

```txt
<game.exe!client.dll>+0x123
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : i32 = 77
<game.exe!client.dll>{ab cd 12 34 ?? ?? ef}
<game.exe!client.dll> : i32 == 42
<game.exe!>{48 8b ?? ?? 89}
<123!> : i32 == 42
```

The grammar should keep these concepts distinct:

```txt
address expression    resolves one address
memory expression     reads or writes one value through a memory view
range expression      resolves one or more ranges
pattern scan          searches ranges for byte patterns
value scan            searches ranges for values matching a view predicate
```

---

## Target References

Current address expressions are process-relative:

```txt
<client.dll>+0x4242
```

The proposed shorthand can embed the process selector in the bracketed target:

```txt
<game.exe!client.dll>+0x4242
<123!client.dll>+0x4242
```

Whole-process ranges use a target with no module name:

```txt
<game.exe!>
<123!>
```

Proposed target forms:

```txt
target_ref :=
    '<' module_name '>'
  | '<' process_selector '!' module_name '>'
  | '<' process_selector '!' '>'

process_selector :=
    pid
  | process_name
```

Examples:

```txt
<client.dll>
<game.exe!client.dll>
<123!client.dll>
<game.exe!>
<123!>
```

Open questions:

- Whether process names need quoting or escaping inside target refs.
- Whether `!` should be reserved and invalid inside process or module names.
- Whether `<123>` should remain a module name rather than becoming a PID target.
  The current proposal avoids that ambiguity by requiring the trailing `!` for
  process-qualified forms.

---

## Address Expressions

The existing pointer-chain model is preserved:

```txt
<game.exe!client.dll>+0x123->(0x4 * 32)->
```

`->` continues to mean pointer dereference only:

```txt
addr->offset means ReadPtr(addr) + offset
addr->       means ReadPtr(addr)
```

Type annotations should not use `->`; they are a layer above address
resolution.

---

## Peek And Poke

Reads use `: view`:

```txt
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123->(0x4 * 32)-> : i32
```

Writes use `: view = value`:

```txt
<game.exe!client.dll>+0x123 : i32 = 77
<game.exe!client.dll>+0x123->(0x4 * 32)-> : i32 = 77
```

Initial write right-hand sides are literals only. RHS expression evaluation was
considered for query-language style workflows, such as copying a value from one
address to another, but is intentionally deferred.

The ambiguous form:

```txt
<game.exe!client.dll>+0x1000 : i32 = <other_game.exe!other_client.dll>+0x2000
```

is not valid in the initial version because the RHS could mean either "write
the numeric address" or "read a value from that address and write it".

Future versions may add explicit value constructors instead of bare RHS address
expressions, such as:

```txt
dst : i32 = val(src : i32)
dst : ptr = addr(src)
```

Proposed dispatch:

```txt
address only       resolve address
address : view     peek value through view
address : view = v poke value through view
```

Open question:

- Whether `<game.exe!client.dll> : i32` should peek at the module base. The
  current proposal says yes because `<client.dll>` already resolves to the
  module base in `spec.md`.

---

## Pattern Scanning

Pattern scans use a range target followed by a braced byte pattern:

```txt
<game.exe!client.dll>{ab cd 12 34 ?? ?? ef}
<game.exe!>{48 8b ?? ?? 89}
<123!>{48 8b ?? ?? 89}
```

Module-relative and dynamic ranges should compose with the same suffix:

```txt
<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}
<game.exe!client.dll>+0x123:+0x500{48 8b ?? ?? 89}
```

Pattern scan result:

```txt
zero or more addresses
```

This replaces the need for an explicit `pscan` verb in the expression language,
though the existing `memmy pscan` subcommand can remain.

Open questions:

- Whether braced patterns should support quoted byte strings.
- Whether pattern flags such as executable-only, readable-only, alignment, and
  result limit belong inside the expression or as REPL/CLI options.

---

## Value Scanning

Value scans use `: view comparison value`:

```txt
<game.exe!client.dll> : i32 == 42
<game.exe!> : i32 > 42
<game.exe!> : i32 == 42
<123!> : i32 == 42
```

The double equals distinguishes scanning from poking:

```txt
<game.exe!client.dll>+0x123 : i32 = 42
```

means write one value.

```txt
<game.exe!client.dll> : i32 == 42
```

means search for matching values.

Comparison operators make value scanning a predicate over candidate addresses:

```txt
<game.exe!> : i32 > 42
<game.exe!> : f32 >= 100.0
<game.exe!client.dll> : u8 != 0
```

Value scan result:

```txt
zero or more addresses
```

Range forms should also compose:

```txt
<game.exe!client.dll>[0x1000:+0x4000] : i32 == 42
<game.exe!client.dll>+0x123:+0x500 : i32 == 42
```

Open questions:

- Which comparison operators belong in the initial version. Candidate set:
  `==`, `!=`, `<`, `<=`, `>`, `>=`.
- Whether ordering comparisons are valid for all views or only numeric views.
- Whether alignment and stride belong in expression syntax or command options.
- Whether scans should default to readable regions only, or require an explicit
  readable filter in whole-process scans.

---

## Strings

Interactive string peeks should be opportunistic and bounded:

```txt
<game.exe!client.dll>+0x123 : str
<game.exe!client.dll>+0x123 : wstr
```

Proposed default stop conditions:

```txt
NUL terminator
first non-printable value
memory read failure
built-in maximum length, such as 1024
```

The built-in limit prevents runaway reads in the REPL and in `--expr` mode.

String views can take a maximum unit count with parentheses:

```txt
<game.exe!client.dll>+0x123 : str(64)
<game.exe!client.dll>+0x123 : wstr(64)
```

Parentheses are used instead of brackets because `view[N]` means `N` elements
of that view. `str[64]` would read like sixty-four strings, not one string with
a maximum length.

Proposed behavior:

```txt
str       narrow printable string, max default units
str(N)    narrow printable string, max N units
wstr      UTF-16LE printable string, max default code units
wstr(N)   UTF-16LE printable string, max N code units
```

For value scans, string literals should probably scan for encoded byte
sequences rather than dynamic string objects:

```txt
<game.exe!> : str == "player_name"
<game.exe!> : wstr == "player_name"
```

Open questions:

- Whether tab, LF, and CR count as printable for `str` and `wstr`.
- Whether `str` means ASCII, current code page, or UTF-8.
- Whether distinct `cstr` and `cwstr` views are needed for required
  NUL-terminated scans.
- Whether string writes should be allowed through `str` and `wstr` views in the
  initial version.

---

## Memory Views And Disassembly

Bracket counts apply to the annotated view:

```txt
view[N] means N elements of view
```

Examples:

```txt
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : u32[4]
<game.exe!client.dll>+0x123 : u8[16]
```

These mean one `i32`, four `u32` values, and sixteen `u8` values respectively.
Raw byte reads use `u8[N]`; a separate `bytes[N]` view is not needed in the
initial version.

Disassembly is a memory view over an address:

```txt
<game.exe!client.dll>+0x1000 : asm
<game.exe!client.dll>+0x1000 : asm[16]
<game.exe!client.dll>+0x1000-> : asm[20]
```

`asm[N]` means decode `N` instructions, not `N` bytes. Bare `asm` uses a
type-specific default instruction count.

Open questions:

- What default instruction count bare `asm` should use.
- Whether byte-bounded disassembly needs a separate future spelling.
- Which disassembly backend to use and how architecture mode is selected.

---

## CLI And REPL Shape

No arguments:

```txt
memmy
```

starts an interactive REPL.

Expression mode:

```txt
memmy --expr "<game.exe!client.dll>+0x123 : i32"
memmy --expr "<game.exe!client.dll>+0x123 : i32 = 77"
memmy --expr "<game.exe!client.dll>{ab cd 12 34 ?? ?? ef}"
memmy --expr "<123!> : i32 == 42"
```

The existing explicit commands can remain:

```txt
memmy peek --pid 1234 --expr '<client.dll>+0x123' --type i32
memmy poke --pid 1234 --expr '<client.dll>+0x123' --type i32 --value 77
memmy scan --pid 1234 --range '<client.dll>' --type i32 --value 42
memmy pscan --pid 1234 --range '<client.dll>' --pattern '48 8b ?? ?? 89'
```

The expression-oriented front end can dispatch by expression shape:

```txt
address_expr               resolve address
address_expr : view        peek
address_expr : view = val  poke
range_expr { pattern }     pattern scan
range_expr : view op val   value scan
```

Shell quoting remains required for `--expr` because `<`, `>`, `{`, `}`, `*`,
and spaces have shell-specific meanings.

---

## Parser Layering

The implementation should probably keep the existing address expression parser
as a lower-level component and add a higher-level expression parser above it.

Possible layering:

```txt
Memmy_AddressExpr
  process-relative address expression

Memmy_TargetExpr
  optional process selector plus module or whole-process target

Memmy_RangeExpr
  one or more resolved memory ranges

Memmy_MemoryExpr
  top-level expression that dispatches to resolve, peek, poke, pattern scan, or
  value scan
```

This keeps the existing API boundary usable for callers that already have a
`Memmy_Process *`, while enabling richer REPL and `--expr` syntax.

Open question:

- Whether process selection should be allowed in the core library parser, or
  only in the CLI/REPL parser that can enumerate and open processes.
