## Project Shape

This repository is a C11 memory introspection toolkit.

- `base/` is the shared foundation library. Prefer its arenas, strings, intrusive containers, filesystem/process helpers, hashing, sorting, regex, bitsets, AVL trees, and OS abstractions before adding new utility code.
- `memmy/` is the core Memmy library. Public project APIs use `Memmy_` names. Private/file-local helpers may use lowercase `memmy_` when a prefix is useful.
- `cmd/memmy/` is the CLI executable. The CMake target is `cmd_memmy`; its output name is `memmy`.
- `vendor/` contains third-party code. Avoid editing it unless the task is explicitly about vendored code.
- `spec.md` is the product and API design reference. Keep it aligned with the actual repo layout and naming.

## Build

- Configure: `cmake -S . -B build`
- Build: `cmake --build build`
- The CLI executable is emitted as `build/cmd/memmy/<config>/memmy.exe` on multi-config Windows generators.

## Code Style

### Naming

- **Types**: `PascalCase`.
- **Memmy public types**: prefixed with `Memmy_`, e.g. `Memmy_Process`, `Memmy_Module`, `Memmy_AddressExpr`.
- **Base types**: no system prefix, e.g. `Arena`, `Scratch`, `String8`, `List`, `HashMap`, `AvlTree`, `BitSet`.
- **Platform-specific types**: prefixed with the platform/system, e.g. `Win32_Process`.
- **Enums**: anonymous enum body with `typedef U32 TypeName` before the enum. Constants are `PascalCase` and prefixed with the enum type:
  ```c
  typedef U32 Memmy_ProcessAccess;
  enum
  {
      Memmy_ProcessAccess_Read  = 1u << 0,
      Memmy_ProcessAccess_Write = 1u << 1,
  };
  ```
- **Functions**: `PascalCase`.
  - Memmy dominant type: `Memmy_Type_Action`, e.g. `Memmy_Process_Open`, `Memmy_AddressExpr_Parse`.
  - Memmy no dominant type: `Memmy_Action`, e.g. `Memmy_ListProcesses`.
  - Base dominant type: `Type_Action`, e.g. `Arena_Push`, `Scratch_Begin`, `String8_Eq`.
  - Base no dominant type: plain `PascalCase`, e.g. `Sort`.
- **Variables and fields**: `snake_case` with no prefix.
- **Constant macros**: `UPPER_CASE`, e.g. `U32_MAX`, `MEMMY_DEFAULT_SCAN_CHUNK_SIZE`.
- **Parameterized macros**: follow function naming, e.g. `ArrayCount`, `Arena_PushStruct`, `Arena_PushArray`.
- **Primitive typedefs**: use base typedefs: `U8`, `U16`, `U32`, `U64`, `I8`, `I16`, `I32`, `I64`, `B32`, `F32`, `F64`.

### Linkage And Includes

- Functions declared in a header have external linkage. Do not mark either the declaration or the `.c` definition `static`.
- Helpers used only in one `.c` file are `static`.
- Each `.c` file includes the headers it uses; each header includes its own dependencies.
- Every header needs a guard.
- Include order: sister header, external headers, local project headers. Use a blank line between groups.

### General Rules

- Formatting is handled by `clang-format`.
- Use C11.
- Do not hide pointers behind typedefs. Spell pointers with `*`.
- Prefer sized base types over C built-in width-ambiguous types.
- Use one declaration per line.
- Null pointer checks use explicit `== 0` and `!= 0`.
- Parameter order: arenas first, primary inputs next, output parameters last.
- Remote process addresses are integers (`Memmy_Addr`), not local pointers, outside platform boundary code.

## Memory And Data Structures

- Allocation should go through `Arena`.
- Use `Scratch` for temporary work. Pass output arenas as conflicts:
  ```c
  Scratch scratch = Scratch_Begin(&out_arena, 1);
  ```
- Prefer zero-initializable structs.
- Prefer base intrusive containers over ad hoc containers:
  - `ListLink` / `List` for ordered collections.
  - `HashLink` / `HashMap` for lookup tables.
  - `AvlLink` / `AvlTree` for ordered maps/sets.
- `List`, `HashMap`, and `AvlTree` fields should have a trailing comment naming the element type, e.g. `List modules; // Memmy_ModuleNode`.
- Use `String8` for string slices and arena-owned text.
- Functions that return variable-length arrays should generally take an `Arena *` first and return a typed slice/list structure, rather than caller-provided buffers.
