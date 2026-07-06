# Memmy DSL Pocket Reference

## Core Values

```txt
x                    constant integer/math expression
@x                   absolute address
[@a..@b]             explicit address range [a, b)
[@a..+n]             sized address range [a, a+n)
<module>             module range in attached/selected process
[0..]                readable regions of attached/selected process
function address     function range containing address
$name                variable
```

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

[0..]                attached process readable regions
<client.dll>         attached process module range
function @0x1234     function range containing address
```

## Addresses

```txt
@0x1234                         absolute address
<client.dll>+0x1234             module base + offset
@0x1234->                       dereference address
@0x1234->->                     chained dereference
@0x1234->0x42                   dereference, then add offset
@0x1234->-0x42                  dereference, then subtract offset
@0x1234->(32 * (4 + 5))         dereference, then add constant expression
[@0x1234..@0x5678]->            dereference start of range
<client.dll>->                  dereference module base
$player->$hp_offset             dereference variable address, then add variable offset
function $xref                  containing function range for address
```

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

Pattern scans, value scans, and reference scans evaluate to address lists.

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
$xrefs => function $
```

`ptr` matches pointer-width little-endian absolute values. `rel32` matches a
signed 32-bit displacement where the displacement field address plus 4 plus the
displacement equals the target address. `any` returns the union of both modes.

## Indexing Address Lists

```txt
<client.dll>{ab cd ? ? 12 34}[0]
(<client.dll> as f32 == 42.777)[2]

$matches = <client.dll>{aa bb ?? ?? 11 22}
$matches[3]
```

## Assignments

Assignments evaluate immediately and bind the resulting value.

```txt
$foo = 42
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
/exit                exit
/quit                exit
```

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
@x                   address
[@a..@b]             explicit address range
[@a..+n]             sized address range
<module>             attached/selected process module range
[0..]                attached/selected process readable regions
range{pattern}       pattern scan -> address list
range as T == value  value scan -> address list
range refs ptr addr  pointer reference scan -> address list
range refs rel32 addr rel32 reference scan -> address list
range refs any addr  ptr or rel32 reference scan -> address list
function address     function range containing address
address as T         typed read
address as T = value typed write
$name = expr         bind evaluated result
$name[N]             index address list
/attach process      select process and clear variables
/detach              clear selected process and variables
/command             control REPL
```
