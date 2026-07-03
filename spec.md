# Memory Introspection Toolkit Specification

## 1. Overview

This project provides a native memory introspection toolkit for inspecting and modifying the virtual memory of local processes.

The primary user interface is a single executable with subcommands:

```txt
memmy procs
memmy mods  --pid 1234
memmy addr  --pid 1234 --expr '<client.dll>+0x4242->0x123->0x4'
memmy peek  --pid 1234 --expr '<client.dll>+0x4242->0x123->0x4' --type u32
memmy poke  --pid 1234 --expr '<client.dll>+0x4242->0x123->0x4' --type u32 --value 1337
memmy scan  --pid 1234 --range '<client.dll>' --type u32 --value 1337
memmy pscan --pid 1234 --range '<client.dll>' --pattern '48 8B ?? ?? 89'
```

The implementation is split into:

```txt
Base layer
  Primitive types, arenas, strings, arrays, formatting, utilities.

Memmy core library
  Process-agnostic memory model, address expression parser, pattern parser,
  scanner, type parser, command-independent data structures.

Platform backend layer
  Isolated OS-specific implementations for process enumeration, process
  opening, remote memory read/write, region enumeration, and module
  enumeration.

CLI layer
  Subcommand parsing, help text, text output, JSON output, command dispatch.
```

Windows is the first supported platform. Linux and macOS are future targets.

The major design constraint is that the address expression language is shared across all tools that accept addresses.

---

## 2. Naming Conventions

The code follows this style:

### 2.1 Types

Types use `PascalCase`.

Structs are public and transparent by default. Use opaque structs only when
there is a concrete reason to hide representation.

Project-specific types are prefixed with `Memmy_`. File names and private C
symbols in the core library use the lowercase `memmy_` prefix where a prefix is
needed.

System-specific types are prefixed with the system name:

```c
Memmy_Process
Memmy_Module
Memmy_Region
Memmy_AddressExpr
Memmy_Pattern
Memmy_ScanResult
CLI_Command
Win32_Process
```

Base layer types have no system prefix:

```c
Arena
Scratch
String8
String8List
BitSet
```

Primitive typedefs:

```c
typedef unsigned char      U8;
typedef unsigned short     U16;
typedef unsigned int       U32;
typedef unsigned long long U64;

typedef signed char        I8;
typedef signed short       I16;
typedef signed int         I32;
typedef signed long long   I64;

typedef U32                B32;
typedef float              F32;
typedef double             F64;
```

### 2.2 Enums

Enums use an anonymous enum body with a typedef before the enum.

Enum constants are `PascalCase`, prefixed by the enum type name.

```c
typedef U32 Memmy_Access;
enum
{
    Memmy_Access_Read    = 1u << 0,
    Memmy_Access_Write   = 1u << 1,
    Memmy_Access_Execute = 1u << 2,
};
```

### 2.3 Functions

Functions use `PascalCase`.

System layer, dominant type:

```c
Memmy_Process_Open
Memmy_Process_Close
Memmy_Process_IsOpen
Memmy_Process_Read
Memmy_Process_Write
Memmy_ModuleList_FindByName
Memmy_ModuleList_FindByAddress
Memmy_AddressExpr_Parse
Memmy_AddressExpr_Resolve
Memmy_Pattern_Parse
Memmy_Pattern_Match
```

System layer, no dominant type:

```c
Memmy_ListProcesses
Memmy_FormatAddress
Memmy_ParseType
```

Base layer, dominant type:

```c
Arena_Push
Arena_Clear
String8_Eq
String8_StartsWith
Scratch_Begin
Scratch_End
```

Base layer, no dominant type:

```c
Sort
ToLower
AlignPow2
AddU64Checked
SubU64Checked
AddI64ToU64Checked
AddI64Checked
SubI64Checked
MulI64Checked
DivI64Checked
ModI64Checked
```

Checked arithmetic helpers return `1` when the result is representable and
written to `out`, and `0` on overflow or underflow. They are declared in
`base_checked.h`:

```c
B32 AddU64Checked(U64 a, U64 b, U64 *out);
B32 SubU64Checked(U64 a, U64 b, U64 *out);
B32 AddI64ToU64Checked(U64 a, I64 b, U64 *out);
B32 AddI64Checked(I64 a, I64 b, I64 *out);
B32 SubI64Checked(I64 a, I64 b, I64 *out);
B32 MulI64Checked(I64 a, I64 b, I64 *out);
B32 DivI64Checked(I64 a, I64 b, I64 *out);
B32 ModI64Checked(I64 a, I64 b, I64 *out);
```

Overflow-sensitive integer parsing, constant folding, address arithmetic, and
range sizing should use these helpers rather than unchecked C arithmetic.

### 2.4 Variables and Fields

Variables and struct fields use `snake_case` with no prefix.

```c
typedef struct Memmy_Module
{
    String8 name;
    String8 path;
    U64 base;
    U64 size;
} Memmy_Module;
```

### 2.5 Macros

Constant macros use `UPPER_CASE`:

```c
#define Kilobytes(n) ((U64)(n) << 10)
#define Megabytes(n) ((U64)(n) << 20)
#define Gigabytes(n) ((U64)(n) << 30)

#define U32_MAX UINT32_MAX
```

Parameterized macros follow function naming rules:

```c
ArrayCount
Arena_PushStruct
Arena_PushArray
AssertTrue
```

---

## 3. Source Layout

```txt
memmy/
  CMakeLists.txt
  justfile
  spec.md

  base/
    include/
      base_core.h
      base_checked.h
      base_arena.h
      base_list.h
      base_hashmap.h
      base_avl.h
      base_string.h
      ...
    src/
      base_checked.c
      base_arena.c
      base_list.c
      base_hashmap.c
      base_avl.c
      base_string.c
      ...

  memmy/
    CMakeLists.txt
    include/
      memmy.h
      memmy_core.h
      memmy_process.h
      memmy_module.h
      memmy_region.h
      memmy_address_expr.h
      memmy_pattern.h
      memmy_scan.h
      memmy_type.h
      memmy_error.h
      memmy_backend.h
    src/
      memmy_address_expr.c
      memmy_pattern.c
      memmy_scan.c
      memmy_type.c
      memmy_format.c
      platform/
        win32/
          memmy_win32_process.c
          memmy_win32_memory.c
          memmy_win32_module.c
          memmy_win32_region.c
        linux/
          memmy_linux_process.c
          memmy_linux_memory.c
          memmy_linux_module.c
          memmy_linux_region.c
        macos/
          memmy_macos_process.c
          memmy_macos_memory.c
          memmy_macos_module.c
          memmy_macos_region.c

  cmd/
    CMakeLists.txt
    memmy/
      CMakeLists.txt
      memmy_main.c
      cli_help.c
      cli_json.c
      cmd_addr.c
      cmd_peek.c
      cmd_poke.c
      cmd_scan.c
      cmd_pscan.c
      cmd_mods.c
      cmd_procs.c

  vendor/
    CMakeLists.txt
    zydis/

  test/
    test_memmy_backend.h
    test_memmy_backend.c
    test_address_expr.c
    test_pattern.c
    test_scan.c
```

---

## 4. Core Concepts

### 4.1 Context

Platform-touching APIs dispatch through the calling thread's current
`Memmy_Context`.

```c
typedef struct Memmy_Context
{
    Memmy_Backend *backend;
} Memmy_Context;
```

Required functions:

```c
Memmy_Context *Memmy_Context_Get(void);
void Memmy_Context_Set(Memmy_Context *ctx);
Memmy_Context *Memmy_Context_Push(Memmy_Context *ctx);
void Memmy_Context_Pop(Memmy_Context *old_ctx);

Memmy_Status Memmy_Context_InitDefault(Arena *arena,
                                       Memmy_Context *ctx,
                                       Memmy_Error *error);
```

`Memmy_Context_Get` returns the current thread-local context. `Memmy_Context_Set`
sets the current context for the calling thread only.
`Memmy_Context_Push` stores the current thread-local context, sets `ctx` as the
current context, and returns the previous context. `Memmy_Context_Pop` restores
a context returned by `Memmy_Context_Push`.

There is no implicit lazy context allocation. CLI and test entry points must
initialize and set a context before calling APIs that require platform access.
Calling a platform-touching API without a current context or without
`ctx->backend` is `Memmy_Status_InvalidArgument`.

`Memmy_Context_InitDefault` initializes `ctx` with the native backend for the
current host platform.

`Memmy_Process_Open` stores the selected backend pointer in the arena-owned
process wrapper. Once a process is open, process operations dispatch through the
backend stored in that process rather than the current thread-local context.

Example CLI setup:

```c
Memmy_Context ctx = {0};
Memmy_Context_InitDefault(arena, &ctx, &error);
Memmy_Context_Set(&ctx);
```

Example test setup:

```c
Test_MemmyBackend *test_backend = Test_MemmyBackend_Create(arena);

Memmy_Context ctx = {0};
ctx.backend = Test_MemmyBackend_AsBackend(test_backend);
Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

/* test */

Memmy_Context_Pop(old_ctx);
```

---

### 4.2 Remote Addresses

Remote process addresses are represented as integers, not local pointers.

```c
typedef U64 Memmy_Addr;
typedef U64 Memmy_Size;
```

A remote address must not be represented as `void *` outside of the platform layer.

Correct:

```c
Memmy_Addr addr;
```

Incorrect:

```c
void *remote_addr;
```

Platform backends are responsible for casting `Memmy_Addr` to the required OS-specific pointer type at the boundary.

---

### 4.3 Process Handle

The process type is public API surface.

```c
typedef struct Memmy_Process Memmy_Process;
struct Memmy_Process
{
    Memmy_Backend *backend;
    U32 pid;
    Memmy_PointerWidth pointer_width;
    void *backend_data;
};
```

`Memmy_Process` objects are allocated from a caller-provided arena. Closing a
process releases platform resources attached to the wrapper, but does not free
the wrapper memory; that memory lives until the arena is reset or destroyed.

`backend`, `pid`, and `pointer_width` are common process metadata. Callers may
read these fields. Callers should not mutate `backend` or `backend_data`.

`backend_data` is backend-private state and must not be interpreted outside the
owning backend. Backend-specific payloads are allocated from the caller-provided
arena.

`Memmy_Process_Open` sets `backend` from the current thread-local
`Memmy_Context`. Process-bound operations dispatch through `process->backend`.

Example Windows backend payload:

```c
typedef struct Win32_Process
{
    void *handle;
    U32 pid;
    U32 pointer_size;
} Win32_Process;
```

---

### 4.4 Pointer Width

The target process pointer width is explicit.

```c
typedef U32 Memmy_PointerWidth;
enum
{
    Memmy_PointerWidth_Unknown,
    Memmy_PointerWidth_32,
    Memmy_PointerWidth_64,
};
```

Pointer reads in address expression resolution use the target process pointer width, not the host pointer width.

---

### 4.5 Endianness

Initial support assumes little-endian targets.

```c
typedef U32 Memmy_Endian;
enum
{
    Memmy_Endian_Little,
    Memmy_Endian_Big,
};
```

Big-endian support is not required for the first implementation.

---

## 5. Error Model

Errors are explicit and non-global.

```c
typedef U32 Memmy_Status;
enum
{
    Memmy_Status_Ok,

    Memmy_Status_InvalidArgument,
    Memmy_Status_NotFound,
    Memmy_Status_Ambiguous,
    Memmy_Status_AccessDenied,
    Memmy_Status_PartialRead,
    Memmy_Status_PartialWrite,
    Memmy_Status_Unreadable,
    Memmy_Status_Unwritable,
    Memmy_Status_ParseError,
    Memmy_Status_Overflow,
    Memmy_Status_InvalidEncoding,
    Memmy_Status_Unsupported,
    Memmy_Status_PlatformError,
    Memmy_Status_OutOfMemory,
};
```

```c
typedef struct Memmy_Error
{
    Memmy_Status status;
    U32 os_code;

    String8 message;

    String8 input;
    U64 byte_offset;
    U64 byte_count;

    String8 context;
} Memmy_Error;
```

Most fallible functions return `Memmy_Status` and optionally fill `Memmy_Error`.

```c
Memmy_Status Memmy_Process_Read(Memmy_Process *process,
                            Memmy_Addr addr,
                            void *buffer,
                            U64 size,
                            U64 *bytes_read,
                            Memmy_Error *error);
```

Guidelines:

* `Memmy_Status_Ok` means the operation fully succeeded.
* `Memmy_Status_PartialRead` means at least one byte was read, but less than requested.
* `Memmy_Status_Unreadable` means the address range could not be read.
* `Memmy_Status_Ambiguous` means a name matched multiple possible targets.
* `Memmy_Status_Overflow` means integer parsing, constant folding, address arithmetic, or range sizing overflowed or underflowed.
* `Memmy_Status_InvalidEncoding` means bytes could not be decoded or encoded using the requested string encoding.
* `Memmy_Status_PlatformError` means an OS-specific failure occurred and `os_code` should be set.

`message` is a human-readable diagnostic. It may be static or arena-owned.

`context` identifies the subsystem that produced the error. Required initial
contexts:

```txt
address_expr
range_expr
type
value
pattern
backend
cli
```

`input`, `byte_offset`, and `byte_count` are used for parser and CLI argument
errors. `byte_offset` is a byte offset into `input`. `byte_count` is the length
of the relevant span. If no span is available, `byte_offset` is `STRING8_NPOS`
and `byte_count` is `0`.

Text CLI errors should include source context when an input span is available:

```txt
error: invalid address expression at byte 12: expected offset
  <client.dll>+
              ^
```

JSON errors use a stable object:

```json
{
  "ok": false,
  "error": {
    "status": "parse_error",
    "message": "expected offset",
    "context": "address_expr",
    "input": "<client.dll>+",
    "byte_offset": 12,
    "byte_count": 1,
    "os_code": 0
  }
}
```

Exit codes:

```txt
0  success
1  general failure
2  invalid command line, parse error, or value encoding error
3  target not found or ambiguous target
4  access denied
5  partial read or partial write
6  unsupported operation
7  platform error
```

---

## 6. Platform Backend

The platform backend abstracts OS-specific process and remote-memory
operations. It is a strict boundary: native OS SDK headers and direct native OS
API calls belong under `memmy/src/platform/<os>/` only.

Portable local OS operations should use the base library's existing wrappers:

```txt
Os_*       virtual memory, basic file I/O, stdout/stderr, environment,
           temporary directories, local process id, raw process spawning
Fs_*       path manipulation, recursive file walking, temporary file helpers
Process_*  higher-level local process execution and pipelines
```

The platform backend may call native OS APIs directly only for operations that
base does not abstract, such as opening another process, reading or writing
remote memory, enumerating another process's modules, and enumerating another
process's virtual memory regions.

Non-platform `memmy/` core files and `cmd/memmy/` CLI files should not include
Windows, Linux, or macOS SDK headers directly. They interact with platform
functionality through `Memmy_Backend` and use `Os_*`, `Fs_*`, or `Process_*`
for local host operations.

Backend-specific state is private to the backend implementation and hangs off
`Memmy_Process.backend_data`. Public code uses the common `Memmy_Process`
fields, `Memmy_Backend`, and integer `Memmy_Addr` values. Casts between
`Memmy_Addr` and native pointer types happen only inside platform backend code.

Platform-touching top-level APIs use the current thread-local `Memmy_Context`.
Process operations use the backend pointer stored in the `Memmy_Process` by
`Memmy_Process_Open`.

```c
typedef struct Memmy_Backend Memmy_Backend;

typedef struct Memmy_Backend
{
    String8 name;

    U32 capabilities;

    Memmy_Status (*list_processes)(Arena *arena,
                                 Memmy_ProcessList *out,
                                 Memmy_Error *error);

    Memmy_Status (*open_process)(Arena *arena,
                               U32 pid,
                               Memmy_ProcessAccess access,
                               Memmy_Process **out,
                               Memmy_Error *error);

    void (*close_process)(Memmy_Process *process);

    Memmy_Status (*read)(Memmy_Process *process,
                       Memmy_Addr addr,
                       void *buffer,
                       U64 size,
                       U64 *bytes_read,
                       Memmy_Error *error);

    Memmy_Status (*write)(Memmy_Process *process,
                        Memmy_Addr addr,
                        void *buffer,
                        U64 size,
                        U64 *bytes_written,
                        Memmy_Error *error);

    Memmy_Status (*list_modules)(Arena *arena,
                               Memmy_Process *process,
                               Memmy_ModuleList *out,
                               Memmy_Error *error);

    Memmy_Status (*list_regions)(Arena *arena,
                               Memmy_Process *process,
                               Memmy_RegionList *out,
                               Memmy_Error *error);
} Memmy_Backend;
```

### 6.1 Backend Capabilities

```c
typedef U32 Memmy_BackendCap;
enum
{
    Memmy_BackendCap_Read        = 1u << 0,
    Memmy_BackendCap_Write       = 1u << 1,
    Memmy_BackendCap_ListProcs   = 1u << 2,
    Memmy_BackendCap_ListModules = 1u << 3,
    Memmy_BackendCap_ListRegions = 1u << 4,
    Memmy_BackendCap_Protect     = 1u << 5,
};
```

The CLI should produce clear errors when a command requires an unsupported
capability. `Memmy_BackendCap_Protect` is reserved for future memory protection
changes and is not required by initial `poke`.

---

## 7. Processes

### 7.1 Process Info

```c
typedef struct Memmy_ProcessInfo
{
    ListLink link;
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
} Memmy_ProcessInfo;
```

```c
typedef struct Memmy_ProcessList
{
    List list; // Memmy_ProcessInfo
} Memmy_ProcessList;
```

Lists are zero-initializable and own no memory. List elements are
arena-allocated and embed their own `ListLink`. A given element can be in only
one list through a given link. Use `List_ForEach` and `List_ForEachReverse` for
iteration:

```c
List_ForEach(Memmy_ProcessInfo, process_info, &processes->list, link)
{
    /* ... */
}
```

### 7.2 Process Access

```c
typedef U32 Memmy_ProcessAccess;
enum
{
    Memmy_ProcessAccess_Read  = 1u << 0,
    Memmy_ProcessAccess_Write = 1u << 1,
    Memmy_ProcessAccess_Query = 1u << 2,
};
```

### 7.3 Required Functions

```c
Memmy_Status Memmy_ListProcesses(Arena *arena,
                             Memmy_ProcessList *out,
                             Memmy_Error *error);

Memmy_ProcessInfo *Memmy_ProcessList_Push(Arena *arena,
                                          Memmy_ProcessList *list);

Memmy_Status Memmy_Process_Open(Arena *arena,
                            U32 pid,
                            Memmy_ProcessAccess access,
                            Memmy_Process **out,
                            Memmy_Error *error);

B32 Memmy_Process_IsOpen(Memmy_Process *process);

void Memmy_Process_Close(Memmy_Process *process);
```

`Memmy_Process_Close` releases OS/backend resources only. It does not free the
`Memmy_Process` wrapper. After close, `Memmy_Process_IsOpen(process)` returns
`0`. Implementations must clear or mark native handle state after closing so
repeated close attempts are harmless.

---

## 8. Modules

### 8.1 Module Type

```c
typedef struct Memmy_Module
{
    ListLink link;
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
} Memmy_Module;
```

```c
typedef struct Memmy_ModuleList
{
    List list; // Memmy_Module
} Memmy_ModuleList;
```

### 8.2 Required Functions

```c
Memmy_Status Memmy_Process_ListModules(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_ModuleList *out,
                                   Memmy_Error *error);

Memmy_Module *Memmy_ModuleList_Push(Arena *arena,
                                    Memmy_ModuleList *list);

Memmy_Status Memmy_ModuleList_FindByName(Memmy_ModuleList *modules,
                                         String8 name,
                                         Memmy_Module **out,
                                         Memmy_Error *error);

Memmy_Status Memmy_ModuleList_FindByAddress(Memmy_ModuleList *modules,
                                            Memmy_Addr addr,
                                            Memmy_Module **out,
                                            Memmy_Error *error);
```

Module names in address and range expressions are matched against
`Memmy_Module.name`, not full module paths. The initial version does not support
path-qualified module disambiguation.

Module name lookup is case-insensitive on Windows and case-sensitive on Linux
and macOS. If no module matches, lookup returns `Memmy_Status_NotFound`. If
multiple modules match, lookup returns `Memmy_Status_Ambiguous` rather than
choosing arbitrarily.

Address lookup returns the module whose half-open range `[base, base + size)`
contains the address. If no module contains the address, it returns
`Memmy_Status_NotFound`. If multiple modules contain the address, it returns
`Memmy_Status_Ambiguous`.

---

## 9. Memory Regions

### 9.1 Region Type

```c
typedef U32 Memmy_RegionAccess;
enum
{
    Memmy_RegionAccess_Read    = 1u << 0,
    Memmy_RegionAccess_Write   = 1u << 1,
    Memmy_RegionAccess_Execute = 1u << 2,
    Memmy_RegionAccess_Guard   = 1u << 3,
};
```

```c
typedef U32 Memmy_RegionState;
enum
{
    Memmy_RegionState_Unknown,
    Memmy_RegionState_Free,
    Memmy_RegionState_Reserved,
    Memmy_RegionState_Committed,
};
```

```c
typedef struct Memmy_Region
{
    ListLink link;
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
} Memmy_Region;
```

```c
typedef struct Memmy_RegionList
{
    List list; // Memmy_Region
} Memmy_RegionList;
```

Regions are half-open ranges `[base, base + size)`. Backends must not emit
zero-sized regions. Backend region lists should be sorted by `base` ascending
and non-overlapping for the initial version.

### 9.2 Required Functions

```c
Memmy_Status Memmy_Process_ListRegions(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_RegionList *out,
                                   Memmy_Error *error);

Memmy_Region *Memmy_RegionList_Push(Arena *arena,
                                    Memmy_RegionList *list);

Memmy_Status Memmy_RegionList_FindByAddress(Memmy_RegionList *regions,
                                            Memmy_Addr addr,
                                            Memmy_Region **out,
                                            Memmy_Error *error);

B32 Memmy_Region_IsReadable(Memmy_Region *region);
B32 Memmy_Region_IsWritable(Memmy_Region *region);
B32 Memmy_Region_IsExecutable(Memmy_Region *region);
```

`Memmy_RegionList_FindByAddress` returns the region containing `addr`. If no
region contains the address, it returns `Memmy_Status_NotFound`. If multiple
regions contain the address, it returns `Memmy_Status_Ambiguous`, though valid
backend output should avoid overlap.

`Memmy_Region_IsReadable`, `Memmy_Region_IsWritable`, and
`Memmy_Region_IsExecutable` return false for guard, free, or reserved regions
even if platform access bits are unusual.

Scanners consider only committed readable regions by default. Scan flags further
filter those candidate regions.

---

## 10. Reading and Writing Memory

### 10.1 Read

```c
Memmy_Status Memmy_Process_Read(Memmy_Process *process,
                            Memmy_Addr addr,
                            void *buffer,
                            U64 size,
                            U64 *bytes_read,
                            Memmy_Error *error);
```

### 10.2 Write

```c
Memmy_Status Memmy_Process_Write(Memmy_Process *process,
                             Memmy_Addr addr,
                             void *buffer,
                             U64 size,
                             U64 *bytes_written,
                             Memmy_Error *error);
```

The initial version does not change remote memory protection.
`Memmy_Process_Write` attempts a direct backend write and reports the result. It
does not temporarily make pages writable.

If the backend or OS can identify a write failure as protection-related, return
`Memmy_Status_Unwritable`. If the process handle lacks required rights, return
`Memmy_Status_AccessDenied`. Other native failures use
`Memmy_Status_PlatformError`.

The CLI `poke` command does not require region enumeration in the initial
version. It may report writability from region data if that data is already
available, but it should not require `ListRegions` just to perform a write.
`poke --dry-run` reads the old value and reports the planned write without
changing memory.

### 10.3 Pointer Read

Pointer reads are used by the address expression resolver.

```c
Memmy_Status Memmy_Process_ReadPtr(Memmy_Process *process,
                               Memmy_Addr addr,
                               Memmy_PointerWidth pointer_width,
                               Memmy_Addr *out,
                               Memmy_Error *error);
```

For `Memmy_PointerWidth_32`, this reads `U32` and zero-extends to `Memmy_Addr`.

For `Memmy_PointerWidth_64`, this reads `U64`.

---

## 11. Address Expressions

Address expressions provide a flexible syntax for identifying remote addresses.

Examples:

```txt
0x10000abcd
<client.dll>+0x4242
0x100001234->0x123
<client.dll>+0x4242->0x123->0x4
<client.dll>+0x4242->(8 * 0x30)->(-0x4)
```

### 11.1 Semantics

A plain address resolves directly:

```txt
0x10000abcd
```

A module expression resolves relative to module base:

```txt
<client.dll>+0x4242
```

means:

```txt
module_base("client.dll") + 0x4242
```

A dereference step:

```txt
addr->offset
```

means:

```txt
ReadPtr(addr) + offset
```

Therefore:

```txt
<client.dll>+0x4242->0x123->0x4
```

means:

```txt
addr = module_base("client.dll") + 0x4242
addr = ReadPtr(addr) + 0x123
addr = ReadPtr(addr) + 0x4
```

Offsets may be parenthesized constant expressions. Constant expressions use
standard arithmetic precedence: parentheses, unary `+`/`-`, `*`/`/`/`%`, then
`+`/`-`, with binary operators associating left-to-right.

```txt
<client.dll>+0x4242->(8 * 0x30)->(-0x4)
```

means:

```txt
addr = module_base("client.dll") + 0x4242
addr = ReadPtr(addr) + 0x180
addr = ReadPtr(addr) - 0x4
```

A bare dereference is also valid:

```txt
<client.dll>+0x4242->
```

meaning:

```txt
addr = ReadPtr(module_base("client.dll") + 0x4242)
```

### 11.2 Grammar

```txt
address_expr  := base address_op*

base          := integer
               | module

module        := '<' module_name '>'

address_op    := add
               | sub
               | deref
               | deref_offset

add           := '+' offset
sub           := '-' offset
deref         := '->'
deref_offset  := '->' offset

offset        := integer
               | '(' const_expr ')'

const_expr    := sum
sum           := product (('+' | '-') product)*
product       := unary (('*' | '/' | '%') unary)*
unary         := ('+' | '-') unary
               | integer
               | '(' const_expr ')'

integer       := hex_integer
               | decimal_integer

hex_integer   := ('0x' | '0X') [0-9a-fA-F]+
decimal_integer := [0-9]+
```

Module names are always bracketed. `module_name` must be non-empty and must not
contain `>`. Leading or trailing whitespace inside the brackets is invalid.

Whitespace is allowed inside parenthesized constant expressions. Whitespace is
not otherwise allowed in address expressions for the initial version.

Integers are unsigned tokens. Negative offsets are expressed only through
parenthesized constant expressions:

```txt
<client.dll>->(-0x4)
```

Constant expressions used as address offsets must evaluate to a value
representable as `I64`. For example, `<client.dll>+(0xffffffffffffffff)` is
invalid even though the literal fits in `U64`, because the resulting signed
offset does not fit in `I64`.

The following forms are invalid:

```txt
client.dll+0x4242
<client.dll>->-0x4
<client.dll>->+0x4
<client.dll>+(foo)
```

Integer literal overflow, constant-expression overflow, division by zero, and
modulo by zero are parse-time failures. Arithmetic overflow or underflow while
resolving an address is a resolve-time `Memmy_Status_Overflow`. Address
resolution uses checked arithmetic helpers such as `AddU64Checked`,
`SubU64Checked`, and `AddI64ToU64Checked` for operations like
`read_addr + signed_offset`.

Constant expression parsing and evaluation is a shared subsystem used by
address expressions and range expressions.

```c
Memmy_Status Memmy_ConstExpr_ParseAndEval(Arena *arena,
                                          String8 text,
                                          I64 *out,
                                          Memmy_Error *error);
```

`Memmy_ConstExpr_ParseAndEval` evaluates a standalone `const_expr` and returns
`Memmy_Status_ParseError` for invalid syntax, division by zero, or modulo by
zero. Integer literal overflow and arithmetic overflow return
`Memmy_Status_Overflow`. Constant folding uses the signed checked arithmetic
helpers: `AddI64Checked`, `SubI64Checked`, `MulI64Checked`, `DivI64Checked`,
and `ModI64Checked`.

### 11.3 Parsed Representation

```c
typedef U32 Memmy_AddressExprBaseKind;
enum
{
    Memmy_AddressExprBaseKind_Address,
    Memmy_AddressExprBaseKind_Module,
};
```

```c
typedef U32 Memmy_AddressExprOpKind;
enum
{
    Memmy_AddressExprOpKind_Add,
    Memmy_AddressExprOpKind_Sub,
    Memmy_AddressExprOpKind_Deref,
    Memmy_AddressExprOpKind_DerefOffset,
};
```

```c
typedef struct Memmy_AddressExprOp
{
    Memmy_AddressExprOpKind kind;
    I64 value;
} Memmy_AddressExprOp;
```

```c
typedef struct Memmy_AddressExpr
{
    Memmy_AddressExprBaseKind base_kind;

    Memmy_Addr base_addr;
    String8 module_name;

    Memmy_AddressExprOp *ops;
    U64 op_count;
} Memmy_AddressExpr;
```

### 11.4 Resolve Trace

The resolver may optionally produce a trace.

```c
typedef U32 Memmy_AddressTraceStepKind;
enum
{
    Memmy_AddressTraceStepKind_BaseAddress,
    Memmy_AddressTraceStepKind_ModuleBase,
    Memmy_AddressTraceStepKind_Add,
    Memmy_AddressTraceStepKind_Sub,
    Memmy_AddressTraceStepKind_Deref,
    Memmy_AddressTraceStepKind_DerefOffset,
};
```

```c
typedef struct Memmy_AddressTraceStep
{
    Memmy_AddressTraceStepKind kind;
    String8 text;
    Memmy_Addr input_addr;
    Memmy_Addr read_addr;
    Memmy_Addr output_addr;
    I64 value;
} Memmy_AddressTraceStep;
```

```c
typedef struct Memmy_AddressTrace
{
    Memmy_AddressTraceStep *steps;
    U64 step_count;
} Memmy_AddressTrace;
```

### 11.5 Required Functions

```c
Memmy_Status Memmy_AddressExpr_Parse(Arena *arena,
                                 String8 text,
                                 Memmy_AddressExpr *out,
                                 Memmy_Error *error);

Memmy_Status Memmy_AddressExpr_Resolve(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_ModuleList *modules,
                                   Memmy_PointerWidth pointer_width,
                                   Memmy_AddressExpr *expr,
                                   Memmy_Addr *out,
                                   Memmy_AddressTrace *trace,
                                   Memmy_Error *error);
```

If `trace` is null, no trace is produced.

Trace records are resolver events, not parser events. Parenthesized constant
expressions are folded before resolution; trace steps keep the original
operation text in `text` and the folded value in `value`.

For non-dereference steps, `read_addr` is `0`.

For `Memmy_AddressTraceStepKind_Deref`:

```txt
input_addr   address where the pointer was read
read_addr    pointer value read from input_addr
value        0
output_addr  read_addr
```

For `Memmy_AddressTraceStepKind_DerefOffset`:

```txt
input_addr   address where the pointer was read
read_addr    pointer value read from input_addr
value        folded signed offset
output_addr  read_addr + value
```

Example trace for:

```txt
<client.dll>+0x4242->(8 * 0x30)->(-0x4)
```

```txt
ModuleBase   text="<client.dll>"       output=module_base("client.dll")
Add          text="+0x4242"            input=module_base, value=0x4242, output=module_base+0x4242
DerefOffset  text="->(8 * 0x30)"       input=addr, read=ReadPtr(addr), value=0x180, output=read+0x180
DerefOffset  text="->(-0x4)"           input=addr, read=ReadPtr(addr), value=-4, output=read-0x4
```

---

## 12. Data Types

Typed reads, writes, and scans use a shared type system.

```c
typedef U32 Memmy_TypeKind;
enum
{
    Memmy_TypeKind_U8,
    Memmy_TypeKind_U16,
    Memmy_TypeKind_U32,
    Memmy_TypeKind_U64,

    Memmy_TypeKind_I8,
    Memmy_TypeKind_I16,
    Memmy_TypeKind_I32,
    Memmy_TypeKind_I64,

    Memmy_TypeKind_F32,
    Memmy_TypeKind_F64,

    Memmy_TypeKind_Ptr,

    Memmy_TypeKind_Bytes,
    Memmy_TypeKind_Str,
    Memmy_TypeKind_WStr,
};
```

```c
typedef struct Memmy_Type
{
    Memmy_TypeKind kind;
    U64 elem_size;
} Memmy_Type;
```

Required spellings:

```txt
u8
u16
u32
u64
i8
i16
i32
i64
f32
f64
ptr
bytes
str
wstr
```

Required functions:

```c
Memmy_Status Memmy_Type_Parse(String8 text,
                          Memmy_Type *out,
                          Memmy_Error *error);
```

`Memmy_Type.elem_size` is the byte size of one value element:

```txt
u8/i8/bytes/str   1
u16/i16/wstr      2
u32/i32/f32       4
u64/i64/f64       8
ptr               target pointer width in bytes
```

Variable-length payload types (`bytes`, `str`, and `wstr`) still have a known
element size. The total encoded byte length of a concrete value lives on
`Memmy_Value.size`.

### 12.1 Value Parsing and Encoding

`Memmy_Value` is the shared arena-owned encoded representation for values
accepted by `poke` and `scan`.

```c
typedef struct Memmy_Value
{
    Memmy_Type type;
    U8 *bytes;
    U64 size;
} Memmy_Value;
```

`bytes` points to arena-owned encoded bytes. `size` is the number of encoded
bytes. `type` is retained for validation, decoding, and output formatting.

Required function:

```c
Memmy_Status Memmy_Value_Parse(Arena *arena,
                               String8 text,
                               Memmy_Type type,
                               Memmy_PointerWidth pointer_width,
                               Memmy_Value *out,
                               Memmy_Error *error);
```

`poke --type <type> --value <value>` and `scan --type <type> --value <value>`
both parse their command-line value through `Memmy_Value_Parse`.

Scalar types are:

```txt
u8 u16 u32 u64
i8 i16 i32 i64
f32 f64
ptr
```

Integer values accept decimal and hexadecimal forms with an optional leading
sign:

```txt
123
-123
0x7b
-0x7b
```

Integer values must fit exactly in the selected type. Overflow or underflow is
`Memmy_Status_Overflow`.

`ptr` values use the same integer syntax and are encoded using the target
process pointer width. A `ptr` value that does not fit the target pointer width
is `Memmy_Status_Overflow`.

Floating-point values accept decimal forms:

```txt
1
1.5
-1.5
1e-3
```

Hex floats, `nan`, and `inf` are not required for the first implementation.

Initial encoding is little-endian for all scalar values.

Payload types are:

```txt
bytes
str
wstr
```

`bytes` values use strict hexadecimal byte syntax:

```txt
90 90 90
DE AD BE EF
```

Wildcards are not allowed in `bytes` values. Wildcards are only accepted by
pattern scans through `Memmy_Pattern`.

`str` values are UTF-8. `wstr` values are UTF-16LE. String values are encoded
without a trailing NUL in the first implementation.

String values do not process escape sequences in the first implementation. The
argument text is encoded exactly as received from the command line after shell
quoting has been handled. For embedded NULs, control bytes, or byte-exact
payloads, use `--type bytes`.

`peek --type bytes`, `peek --type str`, and `peek --type wstr` require
`--count`. For `bytes` and `str`, `--count` is a byte count. For `wstr`,
`--count` is a UTF-16 code-unit count.

`scan --type bytes`, `scan --type str`, and `scan --type wstr` perform exact
byte-sequence scans after encoding the value. String scans are case-sensitive
and do not include an implicit trailing NUL.

`peek --type str` validates UTF-8. `peek --type wstr` validates UTF-16LE.
Invalid remote string data returns `Memmy_Status_InvalidEncoding`. Embedded NULs
are allowed and are treated as data, not terminators.

Text output escapes control and non-printable characters for decoded strings.
JSON output uses standard JSON string escaping. For `bytes`, `str`, and `wstr`,
JSON includes raw bytes as `hex`; for valid strings it also includes decoded
`value`:

```json
{
  "type": "str",
  "value": "hello\u0000world",
  "hex": "68 65 6c 6c 6f 00 77 6f 72 6c 64"
}
```

`poke --type str` and `poke --type wstr` encode local command-line text. If the
CLI cannot convert the command-line argument into the requested encoding, it
returns `Memmy_Status_InvalidEncoding`.

---

## 13. Pattern Scanning

### 13.1 Pattern Syntax

Pattern syntax is byte-oriented.

Examples:

```txt
48 8B ?? ?? 89
42 3b ? ? 77 9c
DE AD BE EF
```

Wildcard forms:

```txt
?
??
```

Both represent one wildcard byte.

### 13.2 Pattern Representation

```c
typedef struct Memmy_PatternByte
{
    U8 value;
    U8 mask;
} Memmy_PatternByte;
```

Exact byte:

```c
{ 0x48, 0xff }
```

Wildcard byte:

```c
{ 0x00, 0x00 }
```

```c
typedef struct Memmy_Pattern
{
    Memmy_PatternByte *bytes;
    U64 count;
} Memmy_Pattern;
```

### 13.3 Required Functions

```c
typedef U32 Memmy_PatternParseFlags;
enum
{
    Memmy_PatternParseFlag_AllowWildcards = 1u << 0,
};

Memmy_Status Memmy_Pattern_Parse(Arena *arena,
                             String8 text,
                             Memmy_PatternParseFlags flags,
                             Memmy_Pattern *out,
                             Memmy_Error *error);

B32 Memmy_Pattern_Match(Memmy_Pattern *pattern,
                      U8 *data,
                      U64 size,
                      U64 offset);
```

Initial implementation may use a simple linear matcher.

`Memmy_Pattern_Parse` is the shared parser for byte patterns and exact byte
values. `pscan --pattern` passes `Memmy_PatternParseFlag_AllowWildcards`.
`Memmy_Value_Parse` passes `0` when parsing `bytes` values for `scan` and
`poke`, so `?` and `??` are rejected for exact byte values.

Future optimizations may include:

```txt
anchor-byte selection
wildcard-aware Boyer-Moore-Horspool
SIMD fixed-window matching
parallel region scanning
```

---

## 14. Scanning

### 14.1 Scan Options

```c
typedef U32 Memmy_ScanFlags;
enum
{
    Memmy_ScanFlag_ReadableOnly    = 1u << 0,
    Memmy_ScanFlag_WritableOnly    = 1u << 1,
    Memmy_ScanFlag_ExecutableOnly  = 1u << 2,
};
```

```c
typedef struct Memmy_Range
{
    ListLink link;
    Memmy_Addr base;
    Memmy_Size size;
} Memmy_Range;
```

```c
typedef struct Memmy_RangeList
{
    List list; // Memmy_Range
} Memmy_RangeList;
```

```c
typedef struct Memmy_ScanOptions
{
    Memmy_RangeList ranges;

    Memmy_ScanFlags flags;

    U64 chunk_size;
    U64 max_results;
} Memmy_ScanOptions;
```

```c
Memmy_Range *Memmy_RangeList_Push(Arena *arena,
                                  Memmy_RangeList *list);
```

If `options->ranges.list` is empty, scans consider all candidate readable
regions after flag filtering. If it is non-empty, scans consider only the
specified ranges.

### 14.2 Scan Result

```c
typedef struct Memmy_ScanResult
{
    ListLink link;
    Memmy_Addr addr;
} Memmy_ScanResult;
```

```c
typedef struct Memmy_ScanResultList
{
    List list; // Memmy_ScanResult
} Memmy_ScanResultList;
```

```c
Memmy_ScanResult *Memmy_ScanResultList_Push(Arena *arena,
                                            Memmy_ScanResultList *list);
```

### 14.3 Value Scan

```c
Memmy_Status Memmy_Process_ScanValue(Arena *arena,
                                 Memmy_Process *process,
                                 Memmy_RegionList *regions,
                                 Memmy_ScanOptions *options,
                                 Memmy_Value *value,
                                 Memmy_ScanResultList *out,
                                 Memmy_Error *error);
```

### 14.4 Pattern Scan

```c
Memmy_Status Memmy_Process_ScanPattern(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_RegionList *regions,
                                   Memmy_ScanOptions *options,
                                   Memmy_Pattern *pattern,
                                   Memmy_ScanResultList *out,
                                   Memmy_Error *error);
```

### 14.5 Chunking Rules

Scans should:

```txt
1. Enumerate candidate readable regions.
2. Filter by requested access flags.
3. Read memory in large chunks.
4. Scan locally.
5. Preserve overlap between chunks for pattern-boundary matches.
6. Emit absolute address results.
7. Optionally format module-relative addresses at the CLI layer.
```

Default chunk size:

```c
#define MEMMY_DEFAULT_SCAN_CHUNK_SIZE Megabytes(16)
```

---

## 15. CLI

The CLI executable is named:

```txt
memmy
```

The general shape is:

```txt
memmy [global-options] <command> [command-options]
```

Supported commands:

```txt
procs   List processes.
mods    List modules in a process.
addr    Resolve an address expression.
peek    Read process memory.
poke    Write process memory.
scan    Scan process memory for typed values.
pscan   Pattern scan process memory.
```

### 15.1 Global Options

```txt
-p, --pid <pid>          Target process id.
-n, --name <name>        Target process name.
--json                   Emit JSON.
--jsonl                  Emit newline-delimited JSON where applicable.
-q, --quiet              Suppress non-essential output.
-v, --verbose            Emit verbose diagnostic output.
--help                   Show help.
```

`--pid` and `--name` are mutually exclusive.

If `--name` matches multiple processes, the command must fail with
`Memmy_Status_Ambiguous` and list matching PIDs.

Options are single-use unless explicitly documented as repeatable. Repeating a
single-use option is `Memmy_Status_InvalidArgument`.

`--json` and `--jsonl` are mutually exclusive. `--quiet` and `--verbose` are
mutually exclusive.

Command-specific inputs are named options in the initial version. Named options
are the canonical form for automation and agent use.

Future versions may add positional shorthand for common human workflows, but
positional forms must remain aliases for the named-option behavior rather than
separate command semantics.

Common command options:

```txt
-e, --expr <addr-expr>      Address expression.
-t, --type <type>           Value type.
--value <value>             Value text.
--pattern <pattern>         Byte pattern text.
--range <range-expr>        Scan range expression.
--count <count>             Explicit byte or character count.
--filter <text>             Command-specific name filter.
```

`--filter` is single-use in the initial version.

`--range` is single-use for `scan` and `pscan`. If no `--range` is provided,
scan commands use all candidate readable regions after flag filtering.

Scan access filters combine as an intersection. For example, `--readable
--writable` scans only regions that are both readable and writable.

### 15.2 Command Requirements

The CLI checks backend capabilities before opening a process when possible, and
requests the minimal process access needed for the command. Unsupported backend
capabilities produce `Memmy_Status_Unsupported`. OS access failures produce
`Memmy_Status_AccessDenied` when they can be identified.

Initial command requirements:

```txt
Command  Required backend capabilities                 Process access
procs    ListProcs                                     none
mods     ListModules                                   Query
addr     Read; ListModules when expression uses modules
                                                        Read | Query
peek     Read; ListModules when expression uses modules
                                                        Read | Query
poke     Read, Write; ListModules when expression uses modules
                                                        Read | Write | Query
poke-dry-run
         Read; ListModules when expression uses modules Read | Query
scan     Read, ListRegions, ListModules                Read | Query
pscan    Read, ListRegions, ListModules                Read | Query
```

`poke-dry-run` means `poke` with `--dry-run`. `addr` requires `Read` because
address expressions may include pointer dereferences. Absolute address
expressions such as `0x000001d856780004` do not require module enumeration.
Module expressions such as `<client.dll>+0x4242` require `ListModules`.
Normal `poke` requires `Read` in the initial version so old/new display remains
available. `scan` and `pscan` require `ListModules` in the initial version so
module ranges and module-relative output are consistently available.

### 15.3 Command: `procs`

```txt
memmy procs
memmy procs --filter chrome
memmy procs --json
```

Text output:

```txt
PID     ARCH   NAME
1234    x64    game.exe
5678    x64    chrome.exe
```

JSON output:

```json
[
  {
    "pid": 1234,
    "name": "game.exe",
    "path": "C:\\Games\\game.exe",
    "pointer_width": 64
  }
]
```

### 15.4 Command: `mods`

```txt
memmy mods --pid 1234
memmy mods --pid 1234 --filter kernel32
memmy mods --pid 1234 --json
```

Text output:

```txt
BASE                SIZE        NAME
0x00007ff800000000  0x1a4000    game.exe
0x00007ff912340000  0x1f0000    kernel32.dll
```

JSON output:

```json
[
  {
    "name": "game.exe",
    "path": "C:\\Games\\game.exe",
    "base": "0x00007ff800000000",
    "size": "0x1a4000"
  }
]
```

### 15.5 Command: `addr`

```txt
memmy addr --pid 1234 --expr '<client.dll>+0x4242->0x123->0x4'
memmy addr --pid 1234 --expr '<client.dll>+0x4242->0x123->0x4' --json
```

Text output:

```txt
<client.dll>    = 0x00007ff800000000
+ 0x4242        = 0x00007ff800004242
->0x123         read 0x000001d812340000, result 0x000001d812340123
->0x4           read 0x000001d856780000, result 0x000001d856780004

resolved: 0x000001d856780004
```

JSON output:

```json
{
  "address_expr": "<client.dll>+0x4242->0x123->0x4",
  "resolved_address": "0x000001d856780004",
  "trace": [
    {
      "kind": "module_base",
      "text": "<client.dll>",
      "output_address": "0x00007ff800000000"
    },
    {
      "kind": "add",
      "text": "+0x4242",
      "input_address": "0x00007ff800000000",
      "output_address": "0x00007ff800004242",
      "value": 16962
    },
    {
      "kind": "deref_offset",
      "text": "->0x123",
      "input_address": "0x00007ff800004242",
      "read_address": "0x000001d812340000",
      "output_address": "0x000001d812340123",
      "value": 291
    },
    {
      "kind": "deref_offset",
      "text": "->0x4",
      "input_address": "0x000001d812340123",
      "read_address": "0x000001d856780000",
      "output_address": "0x000001d856780004",
      "value": 4
    }
  ]
}
```

### 15.6 Command: `peek`

```txt
memmy peek --pid 1234 --expr <addr-expr> --type <type>
memmy peek --pid 1234 --expr <addr-expr> --type bytes --count <count>
memmy peek --pid 1234 --expr <addr-expr> --type str --count <count>
memmy peek --pid 1234 --expr <addr-expr> --type wstr --count <count>
```

Examples:

```txt
memmy peek --pid 1234 --expr '<client.dll>+0x4242' --type u32
memmy peek --pid 1234 --expr '<client.dll>+0x4242' --type bytes --count 64
memmy peek --pid 1234 --expr '<client.dll>+0x4242->0x8' --type ptr
```

Text output:

```txt
0x000001d856780004: u32 1337  0x00000539
```

JSON output:

```json
{
  "address_expr": "<client.dll>+0x4242",
  "resolved_address": "0x000001d856780004",
  "type": "u32",
  "value": 1337,
  "hex": "0x00000539"
}
```

### 15.7 Command: `poke`

```txt
memmy poke --pid 1234 --expr <addr-expr> --type <type> --value <value>
memmy poke --pid 1234 --expr <addr-expr> --type bytes --value '90 90 90'
memmy poke --pid 1234 --expr <addr-expr> --type str --value 'hello'
memmy poke --pid 1234 --expr <addr-expr> --type wstr --value 'hello'
```

Examples:

```txt
memmy poke --pid 1234 --expr '<client.dll>+0x4242' --type u32 --value 1337
memmy poke --pid 1234 --expr '<client.dll>+0x4242' --type bytes --value '90 90 90'
```

Optional safety flag:

```txt
--dry-run
```

Dry-run output:

```txt
would write:
  process: game.exe 1234
  address: 0x000001d856780004
  type:    u32
  old:     100  / 0x00000064
  new:     1337 / 0x00000539
```

### 15.8 Command: `scan`

```txt
memmy scan --pid 1234 --type <type> --value <value>
memmy scan --pid 1234 --range <range-expr> --type <type> --value <value>
memmy scan --pid 1234 --readable --type u32 --value 1337
memmy scan --pid 1234 --writable --type u32 --value 1337
memmy scan --pid 1234 --executable --type bytes --value '48 8B'
memmy scan --pid 1234 --type str --value 'player_name'
memmy scan --pid 1234 --type wstr --value 'player_name'
```

Examples:

```txt
memmy scan --pid 1234 --type u32 --value 1337
memmy scan --pid 1234 --range '<client.dll>' --type u32 --value 1337
memmy scan --pid 1234 --range '<client.dll>[0x1000..0x5000]' --type u32 --value 1337
```

Text output:

```txt
ADDRESS             MODULE+OFFSET
0x00007ff800004242  <client.dll>+0x4242
0x00007ff800007abc  <client.dll>+0x7abc
```

JSONL output:

```json
{"address":"0x00007ff800004242","module":"client.dll","offset":"0x4242","expr":"<client.dll>+0x4242"}
{"address":"0x00007ff800007abc","module":"client.dll","offset":"0x7abc","expr":"<client.dll>+0x7abc"}
```

### 15.9 Command: `pscan`

```txt
memmy pscan --pid 1234 --pattern <pattern>
memmy pscan --pid 1234 --range <range-expr> --pattern <pattern>
memmy pscan --pid 1234 --executable --pattern <pattern>
```

Examples:

```txt
memmy pscan --pid 1234 --pattern '48 8B ?? ?? 89'
memmy pscan --pid 1234 --range '<client.dll>' --pattern '48 8B ?? ?? 89'
```

Output format matches `scan`.

---

## 16. Range Expressions

Range expressions are used by `scan` and `pscan`.

Valid examples:

```txt
<client.dll>
<client.dll>[0x1000..0x5000]
<client.dll>[0x1000:+0x4000]
0x10000000..0x10010000
<client.dll>+0x1000..<client.dll>+0x5000
<client.dll>+0x1000->0x42:+0x5000
<client.dll>[(8 * 0x30)..(8 * 0x40)]
```

Initial required support:

```txt
module
address_expr..address_expr
address_expr:+size
module[start..end]
module[start:+size]
```

All ranges are half-open:

```txt
start..end means [start, end)
```

Range type:

```c
typedef struct Memmy_RangeExpr
{
    String8 text;
} Memmy_RangeExpr;
```

Required function:

```c
Memmy_Status Memmy_RangeExpr_Resolve(Arena *arena,
                                 Memmy_Process *process,
                                 Memmy_ModuleList *modules,
                                 Memmy_PointerWidth pointer_width,
                                 String8 text,
                                 Memmy_RangeList *out,
                                 Memmy_Error *error);
```

`Memmy_RangeExpr_Resolve` appends resolved ranges to `out`. A single range
expression currently resolves to one range, but the list output supports future
range expressions that expand to multiple ranges.

For a module-only range:

```txt
<client.dll>
```

the range resolves to:

```txt
module.base..module.base + module.size
```

### 16.1 Grammar

```txt
range_expr          := module
                    | module_offset_range
                    | module_sized_range
                    | address_range
                    | address_sized_range

module_offset_range := module '[' const_expr '..' const_expr ']'
module_sized_range  := module '[' const_expr ':+' const_expr ']'
address_range       := address_expr '..' address_expr
address_sized_range := address_expr ':+' size
size                := integer
                    | '(' const_expr ')'
```

`const_expr` is the shared constant-expression grammar from address
expressions and is evaluated through `Memmy_ConstExpr_ParseAndEval`.

Module bracket ranges use module-relative constant offsets only:

```txt
<client.dll>[0x1000..0x5000]
```

means:

```txt
[module_base("client.dll") + 0x1000,
 module_base("client.dll") + 0x5000)
```

Module bracket sized ranges use a module-relative start offset and a
non-negative size:

```txt
<client.dll>[0x1000:+0x4000]
```

means:

```txt
[module_base("client.dll") + 0x1000,
 module_base("client.dll") + 0x1000 + 0x4000)
```

Outside module brackets, `..` separates two independently resolved address
expressions:

```txt
<client.dll>+0x1000->0x42..0x5000
```

means:

```txt
[resolve(<client.dll>+0x1000->0x42), resolve(0x5000))
```

This is unambiguous, but the right endpoint is not relative to the left
endpoint or to `<client.dll>`. Treat `address_expr..address_expr` as an
advanced form for ranges with two explicit independently resolved endpoints.

The `:+` form creates a range from a dynamic start address and a non-negative
constant size:

```txt
<client.dll>+0x1000->0x42:+0x5000
<client.dll>+0x1000->0x42:+(8 * 0x100)
```

means:

```txt
[resolve(<client.dll>+0x1000->0x42),
 resolve(<client.dll>+0x1000->0x42) + 0x5000)
```

For dynamic-start ranges, prefer `address_expr:+size` over
`address_expr..address_expr`.

Size expressions used with `:+` must evaluate to `>= 0`. Module offset ranges
must resolve to `end >= start`. Violations are `Memmy_Status_Overflow`.

---

## 17. Output Formatting

### 17.1 Address Formatting

Addresses are formatted as fixed-width lowercase hex.

For 64-bit targets:

```txt
0x00007ff800004242
```

For 32-bit targets:

```txt
0x00404242
```

### 17.2 Module-Relative Formatting

When possible, output absolute and module-relative addresses:

```txt
0x00007ff800004242  <client.dll>+0x4242
```

Function:

```c
String8 Memmy_FormatAddress(Arena *arena,
                          Memmy_ModuleList *modules,
                          Memmy_Addr addr,
                          Memmy_PointerWidth pointer_width);
```

`Memmy_FormatAddress` returns a copy-paste-valid address expression where
possible:

```txt
<client.dll>+0x4242
```

If no module contains the address, it returns a fixed-width absolute address.
If the containing module name cannot be represented in bracketed module syntax
for the initial grammar, such as a name containing `>`, it also falls back to an
absolute address.

JSON scan results include raw fields and the formatted expression:

```json
{"address":"0x00007ff800004242","module":"client.dll","offset":"0x4242","expr":"<client.dll>+0x4242"}
```

### 17.3 JSON Rules

JSON output should:

```txt
1. Use stable key names.
2. Format addresses as strings.
3. Format sizes as strings when hex is clearer.
4. Avoid pretty tables.
5. Avoid human-only prose.
6. Use the standard error object from the error model for failures.
```

Addresses in JSON are strings to avoid integer precision loss in JavaScript consumers.

Successful JSON output is command-shaped, not enveloped. Successful JSONL output
uses raw result records. JSON and JSONL failures use the standard
`{"ok":false,"error":...}` object from the error model.

Byte arrays in JSON are lowercase two-digit hexadecimal bytes separated by a
single space:

```json
"68 65 6c 6c 6f"
```

Numeric hex fields keep the `0x` prefix. Byte-array hex does not use `0x`
prefixes.

---

## 18. Windows Backend

Windows is the first supported backend.

The initial Windows backend is required to fully support same-bitness targets
and 64-bit hosts inspecting 64-bit or WOW64 32-bit targets. Other
cross-bitness combinations may return `Memmy_Status_Unsupported` or
`Memmy_Status_PlatformError` with a clear diagnostic.

### 18.1 Required APIs

Process operations may use:

```txt
OpenProcess
CloseHandle
ReadProcessMemory or NtReadVirtualMemory
WriteProcessMemory or NtWriteVirtualMemory
VirtualQueryEx or NtQueryVirtualMemory
CreateToolhelp32Snapshot
Process32First/Process32Next
Module32First/Module32Next
QueryFullProcessImageName
IsWow64Process2
```

The first implementation may prefer documented Win32 APIs.

Lower-level NT APIs may be added later behind the same backend interface.

### 18.2 Windows Access Mapping

`Memmy_ProcessAccess_Read` requires:

```txt
PROCESS_VM_READ
PROCESS_QUERY_INFORMATION
```

`Memmy_ProcessAccess_Write` requires:

```txt
PROCESS_VM_WRITE
PROCESS_VM_OPERATION
PROCESS_QUERY_INFORMATION
```

`Memmy_ProcessAccess_Query` requires:

```txt
PROCESS_QUERY_INFORMATION
PROCESS_QUERY_LIMITED_INFORMATION
```

### 18.3 Region Mapping

Windows protection flags are mapped to `Memmy_RegionAccess`.

Readable:

```txt
PAGE_READONLY
PAGE_READWRITE
PAGE_WRITECOPY
PAGE_EXECUTE_READ
PAGE_EXECUTE_READWRITE
PAGE_EXECUTE_WRITECOPY
```

Writable:

```txt
PAGE_READWRITE
PAGE_WRITECOPY
PAGE_EXECUTE_READWRITE
PAGE_EXECUTE_WRITECOPY
```

Executable:

```txt
PAGE_EXECUTE
PAGE_EXECUTE_READ
PAGE_EXECUTE_READWRITE
PAGE_EXECUTE_WRITECOPY
```

Guard:

```txt
PAGE_GUARD
```

Free and reserved regions should not be scanned.

---

## 19. Linux Backend

Linux support is planned after Windows.

Expected APIs/files:

```txt
/proc
/proc/<pid>/maps
process_vm_readv
process_vm_writev
/proc/<pid>/mem fallback
ptrace fallback where necessary
```

The Linux backend should implement the same `Memmy_Backend` interface.

---

## 20. macOS Backend

macOS support is planned after Linux.

Expected APIs:

```txt
proc_listpids
proc_pidpath
task_for_pid
mach_vm_read_overwrite
mach_vm_write
mach_vm_region
```

macOS support is expected to be more constrained because of permissions, SIP, hardened runtime, and code signing requirements.

The CLI should surface permission failures clearly.

---

## 21. Testing

The pure core should be testable without touching real processes.

The test backend is test-only scaffolding and lives in:

```txt
test/test_memmy_backend.h
test/test_memmy_backend.c
```

It implements the same `Memmy_Backend` interface used by real platform
backends. Tests should exercise core behavior through `Memmy_Backend` where
practical instead of reaching into platform-specific implementation details.
Tests install it by assigning `Test_MemmyBackend_AsBackend` to a
`Memmy_Context` and calling `Memmy_Context_Push`. Tests should restore the
previous thread-local context with `Memmy_Context_Pop` before exiting.

The initial test backend may support a single fake process id. It must support:

```txt
fixed memory buffer
fake module list
fake region list
configurable pointer width
read/write operations
```

Important tests:

```txt
address expression parsing
address expression resolution
module lookup
range expression resolution
pattern parsing
pattern matching
typed value parsing
scan chunk boundary behavior
JSON escaping
ambiguous process-name handling
```

Test backend helpers:

```c
typedef struct Test_MemmyBackend Test_MemmyBackend;

Test_MemmyBackend *Test_MemmyBackend_Create(Arena *arena);
Memmy_Backend *Test_MemmyBackend_AsBackend(Test_MemmyBackend *backend);

void Test_MemmyBackend_SetMemory(Test_MemmyBackend *backend,
                                 Memmy_Addr base,
                                 U8 *bytes,
                                 U64 size,
                                 Memmy_PointerWidth pointer_width);

Memmy_Module *Test_MemmyBackend_AddModule(Arena *arena,
                                          Test_MemmyBackend *backend,
                                          String8 name,
                                          Memmy_Addr base,
                                          Memmy_Size size);

Memmy_Region *Test_MemmyBackend_AddRegion(Arena *arena,
                                          Test_MemmyBackend *backend,
                                          Memmy_Addr base,
                                          Memmy_Size size,
                                          Memmy_RegionAccess access,
                                          Memmy_RegionState state);
```

---

## 22. Initial Milestones

### Milestone 1: Core CLI Skeleton

```txt
memmy --help
memmy procs
memmy mods --pid <pid>
```

Includes:

```txt
basic command dispatch
global option parsing
Windows process enumeration
Windows module enumeration
text output
```

### Milestone 2: Memory Read

```txt
memmy peek --pid <pid> --expr <addr-expr> --type <type>
memmy addr --pid <pid> --expr <addr-expr>
```

Includes:

```txt
address expression parser
module-relative resolution
pointer-chain resolution
typed reads
resolve trace
```

### Milestone 3: Memory Write

```txt
memmy poke --pid <pid> --expr <addr-expr> --type <type> --value <value>
memmy poke --pid <pid> --expr <addr-expr> --type <type> --value <value> --dry-run
```

Includes:

```txt
typed value parser
safe old/new display
write support
```

### Milestone 4: Pattern Scan

```txt
memmy pscan --pid <pid> --pattern <pattern>
memmy pscan --pid <pid> --range <range> --pattern <pattern>
```

Includes:

```txt
region enumeration
pattern parser
chunked remote reads
module-relative result formatting
```

### Milestone 5: Value Scan

```txt
memmy scan --pid <pid> --type <type> --value <value>
memmy scan --pid <pid> --range <range> --type <type> --value <value>
```

Includes:

```txt
typed value encoding
chunked scanning
result limiting
```

### Milestone 6: Agent Output

```txt
--json
--jsonl
stable error objects
stable result schemas
```

---

## 23. Non-Goals for Initial Version

The first version does not need:

```txt
kernel memory support
remote machine support
symbol loading
PDB/DWARF support
expression arithmetic outside parenthesized constant offsets
Cheat Engine-style repeated narrowing scans
GUI
scripting language
disassembler
memory protection changes
allocation/freeing in remote process
thread enumeration
thread suspension
```

These may be added later, but they should not complicate the first implementation.

---

## 24. Design Principles

```txt
1. Address expressions are the core product.
2. Remote addresses are integers, not pointers.
3. Platform specifics stay behind Memmy_Backend and under memmy/src/platform/.
4. Portable local OS work uses base Os_*, Fs_*, and Process_* primitives.
5. Native OS APIs are used directly only in platform backend code when base has no matching primitive.
6. The CLI must be pleasant for humans.
7. JSON/JSONL must be stable for agents.
8. Scans should minimize remote reads.
9. Help text should teach the address expression language.
10. Dangerous operations should support dry-run or explain modes.
11. Core parser/scanner logic should be testable with Test_MemmyBackend.
12. Windows comes first, but the API shape must not be Windows-only.
```
