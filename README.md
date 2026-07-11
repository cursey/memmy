# Memmy

Memmy is a C11 memory-introspection toolkit with a small DSL for exploring a
process, scanning its memory, following references, and decoding x64
instructions.

## Example: find a RIP-relative global from a string reference

This example attaches to Counter-Strike 2 and finds the global referenced by
the first RIP-relative `mov` near the function that uses
`IGameSystem::InitAllSystems`.

```txt
# Attach to the process and clear variables from any previous session.
/attach cs2

# Find the address of the string in client.dll.
$strings = <client.dll> as str == "IGameSystem::InitAllSystems"

# Find pointer or relative references to every matching string address.
$string_xrefs = $strings => <client.dll> refs any $

# Map each reference to its containing function, then select the first one.
$init_all_game_systems_fns = $string_xrefs => function $
$init_all_game_systems_fn = $init_all_game_systems_fns[0]

# Scan the first 0x100 bytes of the function for RIP-relative mov instructions.
$first_system_movs = [$init_all_game_systems_fn..+0x100] disasm x64 { mov reg, [rip+disp32] }
$first_system_mov = $first_system_movs[0]

# Resolve the target as instruction + length + signed displacement.
# This mov is 7 bytes long, and its disp32 begins 3 bytes into the instruction.
$first_system_tmp = $first_system_mov + 7 + ($first_system_mov + 3 as i32)

# Print the resolved address.
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
