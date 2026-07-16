## Project Shape

This repository is a C11 memory introspection toolkit.

- The project is organized as a set of first-party libraries, command-line tooling, tests, and third-party dependencies. Consult the root CMake files for the current component layout rather than relying on this document as a directory inventory.
- Prefer the shared base layer's data structures, allocation, strings, filesystem/process helpers, and OS abstractions before adding utility code or calling the C standard library directly. When it has a gap, call it out so we can decide whether the missing capability belongs there.
- Keep public APIs within the naming root for their component. Private/file-local helpers may use a lowercase component prefix when useful.
- Avoid editing third-party code unless the task is explicitly about a vendored dependency.

## Build

- Configure: `cmake -S . -B build`
- Build: `cmake --build build`
- The CLI executable is emitted as `build/cmd/memmy/<config>/memmy.exe` on multi-config Windows generators.

## Code Style

### Naming

- **Types**: `PascalCase`.
- **Library roots**: core and platform-backend symbols use `Memmy_`, AST symbols use `MemmyAst_`, evaluator symbols use
  `MemmyEval_`, and CLI/executable symbols use `MemmyCli_`.
- **First-party types**: use an atomic PascalCase type name after the library root, e.g. `Memmy_Process`,
  `MemmyAst_Node`, `MemmyEval_ResultSink`, and `MemmyCli_OutputWriter`. Do not split a compound type name with
  another underscore.
- **Base types**: no system prefix, e.g. `Arena`, `Scratch`, `String8`, `List`, `HashMap`, `AvlTree`, `BitSet`.
- **Platform-specific types**: prefixed with the platform/system, e.g. `Win32_Process`.
- **Enums**: anonymous enum body with `typedef U32 TypeName` before the enum. Constants are `PascalCase` and prefixed
  with the complete enum type:
  ```c
  typedef U32 Memmy_ProcessAccess;
  enum
  {
      Memmy_ProcessAccess_Read  = 1u << 0,
      Memmy_ProcessAccess_Write = 1u << 1,
  };
  ```
- **Functions**: `PascalCase`.
  - Put the dominant type or semantic thing immediately after the library root, followed by its action and any
    description, e.g. `Memmy_Process_Open`, `Memmy_Process_Enumerate`, `MemmyAst_Expr_Parse`,
    `MemmyEval_Expr_Eval`, and `MemmyCli_Address_Format`.
  - Use the primary semantic object when there is no dominant type, such as `Expr`, `Statement`, `Address`, `Argv`,
    or `Process`; do not introduce generic roots such as `Utility` or `App`.
  - Keep implementation boundaries in the thing: platform helpers use `Memmy_Win32Backend_...` or
    `Memmy_DarwinBackend_...`, and command-executable helpers use `MemmyCli_Main_...`.
  - Base dominant type: `Type_Action`, e.g. `Arena_Push`, `Scratch_Begin`, `String8_Eq`.
  - Base no dominant type: plain `PascalCase`, e.g. `Sort`.
  - Preserve established base vocabulary and atomic compound base types such as `String8List_Join`; standalone
    checked-arithmetic and sort APIs are also exceptions to first-party library-root rules.
- **Variables and fields**: `snake_case` with no prefix.
- **Constant macros**: `UPPER_CASE`, e.g. `U32_MAX`, `MEMMY_DEFAULT_SCAN_CHUNK_SIZE`.
- **Parameterized macros**: follow function naming, e.g. `ArrayCount`, `Arena_PushStruct`, `Arena_PushArray`.
- **Primitive typedefs**: use base typedefs: `U8`, `U16`, `U32`, `U64`, `I8`, `I16`, `I32`, `I64`, `B32`, `F32`, `F64`.

### Linkage And Includes

- Functions declared in a header have external linkage. Do not mark either the declaration or the `.c` definition `static`.
- Helpers used only in one `.c` file are `static`.
- Each `.c` file includes the headers it uses; each header includes its own dependencies.
- Each library that exposes a public API has an umbrella header, such as `base.h` or `memmy.h`, that includes all of its
  public headers. Outside that library, include its API through the umbrella header rather than including individual
  public headers. Within the library, use sister headers and direct dependencies so every public header remains
  self-contained. Add each new public header to its library's umbrella header.
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
- Keep grammar comments in `memmy_ast` synchronized with the parse functions they describe whenever DSL syntax or parser behavior changes.

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
