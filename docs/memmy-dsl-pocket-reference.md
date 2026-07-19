# Memmy DSL Pocket Reference

## Core Values

```txt
x                    constant integer/math expression
nil                  type-neutral absence of a value
@x                   absolute address
[@a..@b]             explicit address range [a, b)
[@a..+n]             sized address range [a, a+n)
<module>             module range in attached/selected process
[0..]                concrete address-space range of attached/selected process
function address     function range containing address
objectbase address   best-effort object base containing address
$name                variable
```

`nil` is a reserved lowercase literal. It prints as `nil`; JSONL expression
output represents it with kind `nil` and JSON value `null`.

## Targets

```txt
<client.dll>          module in attached/selected process
```

`<client.dll>` requires a REPL attachment via `/attach`, or one-shot selection via
`--pid` / `--name`.

All memory operations use the current selected process: module targets,
`[0..]`, dereferences, reads, writes, scans, `/mods`, and `/regions`.

## Constants

```txt
0x1234
1234
0x1234 - 32 * (4 + 5)
```

## Ranges

```txt
[@0x1234..@0x5678]   explicit address range: [0x1234, 0x5678)
[@0x1234..+0x5678]  sized address range:    [0x1234, 0x68ac)

[0..]                attached process address-space range
<client.dll>         attached process module range
function @0x1234     function range containing address
```

`[0..]` resolves on each evaluation to an ordinary, concrete half-open range
covering the selected process's virtual address space. Assignments store that
range value without process provenance.

All range-based memory scans automatically traverse only the intersections with
committed, non-guard regions that are readable. This applies equally to `[0..]`,
explicit ranges, module ranges, and function ranges. Exact reads and writes are
strict: they never skip inaccessible holes or silently continue in another
region.

## Addresses

```txt
@0x1234                         absolute address
<client.dll>+0x1234             module base + offset
0x1234 + <client.dll>           module base + offset
$rva = $hit - <client.dll>      RVA as a plain constant
@0x1234->                       dereference address
@0x1234->->                     chained dereference
@0x1234->0x42                   dereference, then add offset
@0x1234->-0x42                  dereference, then subtract offset
@0x1234->(32 * (4 + 5))         dereference, then add constant expression
[@0x1234..@0x5678]->            dereference start of range
<client.dll>->                  dereference module base
$player->$hp_offset             dereference variable address, then add variable offset
function $xref                  containing function range for address
function (<client.dll>+0x1234)  containing function for module base + offset
objectbase (<client.dll>+0x1234) containing object base for module base + offset
```

Module-relative offsets are ordinary constants. A module target by itself is a
range; in address arithmetic, its start address is used as the module base.

## Reads

```txt
@0x1234 as u32
<client.dll>+0x1234-> as str
$player->$hp_offset as f32
```

## Writes

```txt
@0x1234 as u32 = 0x42
<client.dll>+0x1234-> as wstr = "hello, world"
$player->$hp_offset as f32 = 100.0
```

## Address Lists

Pattern scans, value scans, reference scans, and disassembly scans evaluate to
address lists.

```txt
<client.dll>{AB CD ?? ?? 12 34}
[0..]{48 8B ? ? ? ? ? E8 ? ? ? ?}
[@0x1234..@0x5678]{ab cd ? ? 12 34}

<client.dll> as f32 == 42.777
[0..] as str == "hello"
[@0x1234..@0x5678] as u32 == 123

<client.dll> refs ptr @0x1234
[0..] refs rel32 $target
[@0x1234..@0x5678] refs any <client.dll>+0x20

<client.dll> disasm x64 { mov reg, [rip+disp32]; xor rax, rax }
[0..] disasm x64 { mov rax, [rip+disp32] }
$xrefs => function $
```

Quoted `str` and `wstr` scan/write values use DSL string literal contents, so
`"hello"` scans/writes `hello` without quote bytes. Supported escapes are
`\"`, `\\`, `\n`, `\r`, and `\t`.

`ptr` matches pointer-width little-endian absolute values. `rel32` matches a
signed 32-bit displacement where the displacement field address plus 4 plus the
displacement equals the target address. `any` returns the union of both modes.

`disasm x64` matches consecutive decoded x64 instructions using structured
Zydis instruction and operand data, not formatted disassembly text. The v1
operand forms are:

```txt
reg              any explicit register operand
rax/eax/xmm0     exact explicit register operand
[rip+disp32]     RIP-relative memory operand with a 32-bit displacement
```

Only x64 is supported. Operands are matched against visible explicit operands.
`[rip+disp32]` checks the RIP-relative memory shape only; it does not resolve or
filter the absolute target address.

## Indexing Address Lists

```txt
<client.dll>{ab cd ? ? 12 34}[0]
(<client.dll> as f32 == 42.777)[2]

$matches = <client.dll>{aa bb ?? ?? 11 22}
$matches[3]
$hit = $matches[0] - 0xf
```

## Flow Pipelines With `|>` And `=>`

`|>` evaluates its left side once, binds that complete value to bare `$`, then
evaluates its right side once. The right-side value is returned unchanged: the
pipe adds no type restriction, conversion, flattening, or empty-list error.

```txt
$matches |> $[0]                    select the first match
$address |> $ as u32                read a value through a piped address
$ranges |> $                        pass a range list through unchanged
```

`|>` accepts `nil`, constants, typed values, addresses, ranges, address lists,
range lists, and empty lists. The right side may ignore `$`; the left side is
still evaluated first. Piping `nil` binds `$` to `nil` normally.

`=>` evaluates an expression once for every item in an address list or range
list. Inside the expression on the right, bare `$` means the current flow input
(the current list item). Successful address and range results are collected in
input order into a new list. Failed evaluations and successful `nil` results
are omitted, making `=>` a filter-map rather than a cardinality-preserving map.

```txt
$refs => $ + 4              add 4 to every address
$refs => [$..+0x20]         make a 0x20-byte range at every address
$xrefs => function $        find the containing function for every xref
$hits => objectbase $       find the object base for every hit
```

For example, if `$refs` contains `@0x1000` and `@0x2000`, then:

```txt
$refs => $ + 4
```

produces an address list containing `@0x1004` and `@0x2004`. `$refs` is the
input list, `$ + 4` is the per-item expression, and `$` takes the value of each
address in turn.

The left side must evaluate to an address list, range list, or `nil`; a single
address or range is not accepted. The right side must produce `nil`, addresses,
address lists, ranges, or range lists. If it produces a list, that list is
flattened into the result. All produced items must have the same category:
addresses and ranges cannot be mixed in one result.

Every right-side failure status is suppressed for that item, including lookup,
arithmetic, argument, and backend failures. Evaluation continues with the next
item and produces no diagnostic for the omitted failure. Side effects completed
before a right-side failure are not rolled back. A successful empty list also
contributes no item.

An empty input list returns `nil` without evaluating the right side. If every
right-side evaluation fails, returns `nil`, or returns an empty list, the result
is also `nil`. `nil => expr` returns `nil` without evaluating `expr`.

`|>` and `=>` have the same lowest precedence and form one left-associative flow
chain, so each result becomes the next stage's input:

```txt
$refs => [$..+0x20] => $ + 4
$xrefs => function $ |> $[0]
```

This first makes a range for each address, then transforms each range. In
address arithmetic, a range contributes its start address, so the final result
is an address list containing each original address plus 4.

Parentheses create a nested flow chain. The innermost pipe or transform binding
shadows `$`, and the outer binding is restored after the nested flow succeeds or
fails. For example, this pipe runs separately inside every transform iteration:

```txt
$refs => (function $ |> $ - <client.dll>)
```

`=>` is distinct from `->`: `=>` transforms every item in a list, while `->`
dereferences one address (or the start of one range).

Transform-level structural errors remain fatal: the left side must be a list or
`nil`, successful right-side values must be address/range values, and successful
address and range categories cannot be mixed. Other operations reject `nil`
unless documented otherwise. Indexing `nil`, such as `nil[0]`, reports the
normal index-not-found error.

## Assignments

Assignments evaluate immediately and bind the resulting value.

```txt
$foo = 42
$foo = nil
$foo = @0x1234
$foo = <client.dll>
$foo = [@0x1234..@0x5678]
$foo = $bar
$foo = $bar->0x42->
$foo = <client.dll>+0x1234->->0x42->
$foo = <client.dll>{ab cd ? ? 12 34}[0]
$foo = (<client.dll> as f32 == 42.777)[2]
$foo = <client.dll>{aa bb ?? ?? 11 22}
$fn = function $xref
$fns = $xrefs => function $
```

## Variables

```txt
$foo                 print variable
$foo[0]              index address list variable
```

Variables store only value data. They do not remember which process was selected
when they were assigned, so reading, writing, dereferencing, scanning, or
formatting a stored address uses the current selected process.

## REPL Commands

```txt
/procs               list processes
/procs game          fuzzy-filter processes
/attach game.exe     attach by process name and clear variables
/attach 1234         attach by PID and clear variables
/detach              clear attached process and variables
/mods                list attached process modules
/mods client         fuzzy-filter attached process modules
/regions             list attached process memory regions
/vars                list variables
/unset $var          remove variable
/clear               clear variables
/help                show help
/tutorial            start or repeat the interactive tutorial
/tutorial hint       show a hint for the current lesson
/tutorial restart    reset and restart the tutorial
/tutorial stop       stop the tutorial
/exit                exit
/quit                exit
```

The interactive tutorial is text-only. `/tutorial` returns `invalid_argument`
when `--jsonl` output is selected.

## Example Flow

```txt
/attach game.exe
$anchor = <client.dll>{48 8B ?? ?? ?? ?? ?? E8 ?? ?? ?? ??}[0]
$target = $anchor+7+($anchor+3 as i32)
[0..] as str == "hello"
```

## Mental Model

```txt
x                    constant
nil                  type-neutral absence of a value
@x                   address
[@a..@b]             explicit address range
[@a..+n]             sized address range
<module>             attached/selected process module range
[0..]                attached/selected process address-space range
range{pattern}       pattern scan -> address list
range as T == value  value scan -> address list
range refs ptr addr  pointer reference scan -> address list
range refs rel32 addr rel32 reference scan -> address list
range refs any addr  ptr or rel32 reference scan -> address list
range disasm x64 {...} x64 disassembly scan -> address list
function address     function range containing address
address as T         typed read
address as T = value typed write
$name = expr         bind evaluated result
$name[N]             index address list
value |> expr        bind the complete value to `$` and evaluate expr once
list => expr         filter-map address/range items; failed/nil RHS values are omitted
/attach process      select process and clear variables
/detach              clear selected process and variables
/tutorial            start or control the interactive, read-only tutorial
/command             control REPL
```
