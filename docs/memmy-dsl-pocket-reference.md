# Memmy DSL Pocket Reference

Memmy expressions evaluate to semantic values. Every value has one of these
families: `null`, integer, float, `address`, string, `range`, or `list<T>`.
Scalar exact types are `u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `u64`, `i64`,
`f32`, `f64`, `str`, and `wstr`. Lists are homogeneous and created by scans or
flow transforms; `list<T>` is not user-written syntax.

## Literals and conversions

```txt
42                    i64 integer literal
0x1234                i64 hexadecimal integer literal
42.5                  f64 floating-point literal
"hello\nworld"        str literal
nil                   null value

42 as u8              scalar conversion
42.5 as f32           scalar conversion
"hello" as wstr       string exact-type conversion
@0x1234               address construction
```

String escapes are `\"`, `\\`, `\n`, `\r`, and `\t`. `as` converts
non-address numeric and string scalars. Float values can be converted, stored,
read, scanned, listed, and printed, but float arithmetic is unsupported.

`@integer` is the only integer-to-address operation. Negative and
unrepresentable operands are rejected. Applying `as T` to an address or range
reads memory at the address or range start instead of converting the address.
Nulls and lists cannot be converted with `as`.

## Integer and address arithmetic

```txt
1 + 2 * 3
@0x1000 + 4
4 + @0x1000
@0x1010 - 0x10
@0x1010 - @0x1000      i64 address difference
<client.dll> + 0x20    address at module start plus offset
$rva = $hit - <client.dll>
```

Integer literals are `i64`. Unary and binary operations promote 8- and 16-bit
integers to `i32`, then apply the usual signed/unsigned width rules. Unsigned
results wrap to their result width. Signed overflow, division by zero, modulo
by zero, and signed minimum divided by `-1` fail.

Addresses and ranges contribute an address (a range contributes its start).
They may be offset by checked signed or unsigned integers. Subtracting two
address-like values returns `i64`; other address-like combinations fail.

## Ranges and selected processes

```txt
[@0x1234..@0x5678]    explicit half-open range
[@0x1234..+0x100]     sized half-open range
<client.dll>          selected-process module range
[0..]                 selected-process address-space range
function @0x1234      containing function range
objectbase @0x1234    best-effort containing object address
```

Use `/attach`, `--pid`, or `--name` to select a process. `[0..]` is freshly
resolved on every evaluation. Variables store ordinary address/range values,
not process provenance, so later reads, dereferences, and scans use the process
selected at that time.

Range scans traverse readable intersections of committed, non-guard regions.
Exact reads and pointer dereferences are strict and do not skip inaccessible
holes.

## Reads and pointers

```txt
@0x1234 as u32
[@0x1234..+0x20] as str       reads at range start
$player->$hp_offset as f32
@0x1234->                     read a target-width pointer
@0x1234->0x20                 pointer read, then add offset
@0x1234->->                   chained pointer reads
```

Reads decode immediately into semantic values and retain no source address,
encoded bytes, or process provenance. The DSL has no typed-write syntax; the
core process-write and value-encoding APIs remain available to C callers.

## Scans

All scan forms return `list<address>`.

```txt
<client.dll>{48 8B ?? ?? E8 ?? ?? ?? ??}
[@0x1000..+0x100]{00 00}

<client.dll> as u32 == $needle + 1
[0..] as f32 == 42.777
[0..] as str == "hello"
[0..] as wstr == "hé"

<client.dll> refs ptr @0x1234
[0..] refs rel32 $target
[0..] refs any <client.dll>+0x20

<client.dll> disasm x64 { mov reg, [rip+disp32]; xor rax, rax }
```

A value-scan RHS is a normal expression. It is evaluated once, converted to
the declared exact type, encoded, and scanned. String scans omit the encoded
trailing terminator; use a raw byte pattern to match a terminator explicitly.

`ptr` is only a reference-scan mode. It matches target-pointer-width absolute
references. `rel32` matches signed 32-bit PC-relative displacements, and `any`
matches both. Neither `ptr` nor raw `bytes` is an `as` type.

Disassembly scans currently support x64. Operand forms are `reg` (any explicit
register), an exact register such as `rax` or `xmm0`, and `[rip+disp32]`.

## Lists, indexing, and flows

```txt
$matches[0]                    index any homogeneous list
$matches |> $[0]               bind the whole value to `$` once
$numbers => $ as u16           convert every element
$refs => $ + 4                 map addresses
$refs => [$..+0x20]            map addresses to ranges
$xrefs => function $           map/filter function lookups
$lists => $other_list          flatten one list level
```

`|>` evaluates its left side once, binds that complete value to bare `$`, and
evaluates its RHS once. It accepts every value family, including null and
lists, and returns the RHS unchanged.

`=>` accepts any `list<T>`. Before iteration, Memmy resolves the RHS output
type without opening a process or performing reads. Scalar results accumulate
into a homogeneous list; list results flatten one level. Per-item evaluation
failures and runtime nulls are filtered. All produced element types must match
structurally, and nested lists are rejected.

Typed empty input and all-filtered transforms return an empty `list<T>` with
the resolved output type. `nil => rhs` remains a null short-circuit and does not
resolve or evaluate `rhs`. Parenthesized nested flows shadow `$` and restore the
outer binding afterward.

## Assignments and variables

```txt
$answer = 6 * 7
$name = "player"
$address = @0x1234
$range = <client.dll>
$matches = <client.dll>{48 8B ?? ??}
$values = $matches => $ as u32

$name
$values[0]
/vars
/unset $name
/clear
```

Assignments deep-copy semantic values, including list descriptors, compact
payload arrays, and list strings. `/vars` reports exact types such as `u32`,
`wstr`, and `list<range>`.

## Output

Text scalar output is `TYPE VALUE`. Lists stream indexed items followed by a
typed count summary. JSONL scalars use:

```json
{"type":"value","value_type":"T","value":...}
```

Addresses are hexadecimal strings. Ranges contain `start` and `end` address
fields. Non-finite JSON floats are `null` without changing their in-memory bit
patterns. JSONL lists emit `list_item` records followed by a `summary` record.

## REPL commands

```txt
/procs [filter]       list or fuzzy-filter processes
/attach <pid|name>   select a process and clear variables
/detach              clear the selected process and variables
/mods [filter]       list or fuzzy-filter modules
/regions             list memory regions
/vars                list exact variable types
/unset $var          remove a variable
/clear               clear variables
/help                show the DSL overview
/tutorial [hint|restart|stop]
/exit
/quit
```

## Example

```txt
/attach game.exe
$strings = <client.dll> as str == "IGameSystem::InitAllSystems"
$xrefs = $strings => <client.dll> refs any $
$fn = $xrefs => function $ |> $[0]
$movs = [$fn..+0x100] disasm x64 { mov reg, [rip+disp32] }
$target = $movs[0] + 7 + ($movs[0] + 3 as i32)
$target
```
