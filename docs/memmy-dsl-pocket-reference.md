# Memmy DSL Pocket Reference

## Core Values

```txt
x                    constant integer/math expression
@x                   absolute address
[@a..@b]             explicit address range [a, b)
[@a..+n]             sized address range [a, a+n)
<target>             process/module range
$name                variable
```

## Targets

```txt
<game.exe!>           process by name
<1234!>               process by PID
<client.dll>          module in current process
<game.exe!client.dll> process module by process name
<1234!client.dll>     process module by PID
```

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

<game.exe!>          whole process range
<client.dll>         module range
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

Pattern scans and value scans evaluate to address lists.

```txt
<client.dll>{AB CD ?? ?? 12 34}
<game.exe!>{48 8B ? ? ? ? ? E8 ? ? ? ?}
[@0x1234..@0x5678]{ab cd ? ? 12 34}

<client.dll> as f32 == 42.777
<game.exe!> as str == "hello"
[@0x1234..@0x5678] as u32 == 123
```

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
```

## Variables

```txt
$foo                 print variable
$foo[0]              index address list variable
```

## REPL Commands

```txt
/procs               list processes
/procs game          fuzzy-filter processes
/mods                list modules
/mods client         fuzzy-filter modules
/regions             list memory regions
/vars                list variables
/unset $var          remove variable
/clear               clear variables
/help                show help
/exit                exit
/quit                exit
```

## Example Flow

```txt
$anchor = <client.dll>{48 8B ?? ?? ?? ?? ?? E8 ?? ?? ?? ??}[0]
$target = $anchor+7+($anchor+3 as i32)
```

## Mental Model

```txt
x                    constant
@x                   address
[@a..@b]             explicit address range
[@a..+n]             sized address range
<target>             named process/module range
range{pattern}       pattern scan -> address list
range as T == value  value scan -> address list
address as T         typed read
address as T = value typed write
$name = expr         bind evaluated result
$name[N]             index address list
/command             control REPL
```
