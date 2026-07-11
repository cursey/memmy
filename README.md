# Memmy

Memmy is a C11 memory-introspection toolkit with a small DSL for exploring a
process, scanning its memory, following references, and decoding x64
instructions.

## Example: find a RIP-relative global from a string reference

This example attaches to Counter-Strike 2 and finds the global referenced by
the first RIP-relative `mov` near the function that uses
`IGameSystem::InitAllSystems`.

Attach to the process. Attaching also clears variables from any previous
session:

```txt
/attach cs2
```

Find the address of the string in `client.dll`:

```txt
$strings = <client.dll> as str == "IGameSystem::InitAllSystems"
```

Find pointer or relative references to every matching string address:

```txt
$string_xrefs = $strings => <client.dll> refs any $
```

Map each reference to its containing function, then select the first function:

```txt
$init_all_game_systems_fns = $string_xrefs => function $
$init_all_game_systems_fn = $init_all_game_systems_fns[0]
```

Scan the first `0x100` bytes of that function for x64 instructions shaped like
`mov reg, [rip+disp32]`, then select the first match:

```txt
$first_system_movs = [$init_all_game_systems_fn..+0x100] disasm x64 { mov reg, [rip+disp32] }
$first_system_mov = $first_system_movs[0]
```

Resolve the instruction's RIP-relative displacement. This `mov` is seven bytes
long, and its signed 32-bit displacement begins three bytes into the
instruction, so the target is `instruction + 7 + displacement`:

```txt
$first_system_tmp = $first_system_mov + 7 + ($first_system_mov + 3 as i32)
```

Print the resolved address:

```txt
$first_system_tmp
```

The example deliberately selects index `0` at two points. If several strings,
references, functions, or instructions match, inspect the corresponding list
before choosing the result appropriate for the target build.

## Build

```sh
cmake -S . -B build
cmake --build build
```

On multi-config Windows generators, the CLI is emitted at
`build/cmd/memmy/<config>/memmy.exe`.

For the complete language and REPL command reference, see the
[Memmy DSL pocket reference](docs/memmy-dsl-pocket-reference.md).
