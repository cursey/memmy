# Memory Introspection Toolkit Specification

## 1. Overview

This project provides a native memory introspection toolkit for inspecting and modifying the virtual memory of local processes.

The primary user interface is a single executable with subcommands:

```txt
memmy procs
memmy mods  -p 1234
memmy addr  -p 1234 'client.dll+0x4242->0x123->0x4'
memmy peek  -p 1234 'client.dll+0x4242->0x123->0x4' u32
memmy poke  -p 1234 'client.dll+0x4242->0x123->0x4' u32 1337
memmy scan  -p 1234 --range client.dll u32 1337
memmy pscan -p 1234 --range client.dll '48 8B ?? ?? 89'
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
Memmy_Process_Read
Memmy_Process_Write
Memmy_ModuleList_Find
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
```

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
      base_arena.h
      base_list.h
      base_hashmap.h
      base_avl.h
      base_string.h
      ...
    src/
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
    test_address_expr.c
    test_pattern.c
    test_scan.c
    test_fake_backend.c
```

---

## 4. Core Concepts

### 4.1 Remote Addresses

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

### 4.2 Process Handle

The public process type is opaque.

```c
typedef struct Memmy_Process Memmy_Process;
```

Platform-specific state is hidden behind this type.

Example Windows implementation:

```c
typedef struct Win32_Process
{
    void *handle;
    U32 pid;
    U32 pointer_size;
} Win32_Process;
```

---

### 4.3 Pointer Width

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

### 4.4 Endianness

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
    Memmy_Status_AccessDenied,
    Memmy_Status_PartialRead,
    Memmy_Status_PartialWrite,
    Memmy_Status_Unreadable,
    Memmy_Status_Unwritable,
    Memmy_Status_ParseError,
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
* `Memmy_Status_PlatformError` means an OS-specific failure occurred and `os_code` should be set.

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

Platform-specific state is private to the backend implementation. Public code
uses opaque `Memmy_Process` handles, `Memmy_Backend`, and integer
`Memmy_Addr` values. Casts between `Memmy_Addr` and native pointer types happen
only inside platform backend code.

```c
typedef struct Memmy_Backend Memmy_Backend;

typedef struct Memmy_Backend
{
    String8 name;

    U32 capabilities;

    Memmy_Status (*list_processes)(Arena *arena,
                                 Memmy_ProcessList *out,
                                 Memmy_Error *error);

    Memmy_Status (*open_process)(U32 pid,
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

The CLI should produce clear errors when a command requires an unsupported capability.

---

## 7. Processes

### 7.1 Process Info

```c
typedef struct Memmy_ProcessInfo
{
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
} Memmy_ProcessInfo;
```

```c
typedef struct Memmy_ProcessNode
{
    struct Memmy_ProcessNode *next;
    Memmy_ProcessInfo info;
} Memmy_ProcessNode;

typedef struct Memmy_ProcessList
{
    Memmy_ProcessNode *first;
    Memmy_ProcessNode *last;
    U64 count;
} Memmy_ProcessList;
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

Memmy_Status Memmy_Process_Open(U32 pid,
                            Memmy_ProcessAccess access,
                            Memmy_Process **out,
                            Memmy_Error *error);

void Memmy_Process_Close(Memmy_Process *process);
```

---

## 8. Modules

### 8.1 Module Type

```c
typedef struct Memmy_Module
{
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
} Memmy_Module;
```

```c
typedef struct Memmy_ModuleNode
{
    struct Memmy_ModuleNode *next;
    Memmy_Module module;
} Memmy_ModuleNode;

typedef struct Memmy_ModuleList
{
    Memmy_ModuleNode *first;
    Memmy_ModuleNode *last;
    U64 count;
} Memmy_ModuleList;
```

### 8.2 Required Functions

```c
Memmy_Status Memmy_Process_ListModules(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_ModuleList *out,
                                   Memmy_Error *error);

Memmy_Module *Memmy_ModuleList_FindByName(Memmy_ModuleList *modules,
                                      String8 name);

Memmy_Module *Memmy_ModuleList_FindByAddress(Memmy_ModuleList *modules,
                                         Memmy_Addr addr);
```

Module name lookup is case-insensitive on Windows. Linux and macOS behavior should be defined by the backend, but the CLI should generally try to be forgiving.

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
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
} Memmy_Region;
```

```c
typedef struct Memmy_RegionNode
{
    struct Memmy_RegionNode *next;
    Memmy_Region region;
} Memmy_RegionNode;

typedef struct Memmy_RegionList
{
    Memmy_RegionNode *first;
    Memmy_RegionNode *last;
    U64 count;
} Memmy_RegionList;
```

### 9.2 Required Functions

```c
Memmy_Status Memmy_Process_ListRegions(Arena *arena,
                                   Memmy_Process *process,
                                   Memmy_RegionList *out,
                                   Memmy_Error *error);

B32 Memmy_Region_IsReadable(Memmy_Region *region);
B32 Memmy_Region_IsWritable(Memmy_Region *region);
B32 Memmy_Region_IsExecutable(Memmy_Region *region);
```

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
client.dll+0x4242
<client.dll>+0x4242
0x100001234->0x123
client.dll+0x4242->0x123->0x4
```

### 11.1 Semantics

A plain address resolves directly:

```txt
0x10000abcd
```

A module expression resolves relative to module base:

```txt
client.dll+0x4242
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
client.dll+0x4242->0x123->0x4
```

means:

```txt
addr = module_base("client.dll") + 0x4242
addr = ReadPtr(addr) + 0x123
addr = ReadPtr(addr) + 0x4
```

A bare dereference is also valid:

```txt
client.dll+0x4242->
```

meaning:

```txt
addr = ReadPtr(module_base("client.dll") + 0x4242)
```

### 11.2 Grammar

```txt
address_expr  := base step*

base          := integer
               | module

module        := ident
               | '<' module_name '>'

step          := add
               | sub
               | deref
               | deref_add

add           := '+' integer
sub           := '-' integer
deref         := '->'
deref_add     := '->' integer

integer       := hex_integer
               | decimal_integer

hex_integer   := '0x' [0-9a-fA-F]+
decimal_integer := [0-9]+
```

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
    Memmy_AddressExprOpKind_DerefAdd,
};
```

```c
typedef struct Memmy_AddressExprOp
{
    Memmy_AddressExprOpKind kind;
    U64 value;
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
    Memmy_AddressTraceStepKind_DerefAdd,
};
```

```c
typedef struct Memmy_AddressTraceStep
{
    Memmy_AddressTraceStepKind kind;
    String8 text;
    Memmy_Addr input_addr;
    Memmy_Addr output_addr;
    U64 value;
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
    U64 size;
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

U64 Memmy_Type_Size(Memmy_Type type,
                  Memmy_PointerWidth pointer_width);
```

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
Memmy_Status Memmy_Pattern_Parse(Arena *arena,
                             String8 text,
                             Memmy_Pattern *out,
                             Memmy_Error *error);

B32 Memmy_Pattern_Match(Memmy_Pattern *pattern,
                      U8 *data,
                      U64 size,
                      U64 offset);
```

Initial implementation may use a simple linear matcher.

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
    Memmy_Addr base;
    Memmy_Size size;
} Memmy_Range;
```

```c
typedef struct Memmy_ScanOptions
{
    Memmy_Range *ranges;
    U64 range_count;

    Memmy_ScanFlags flags;

    U64 chunk_size;
    U64 max_results;
} Memmy_ScanOptions;
```

### 14.2 Scan Result

```c
typedef struct Memmy_ScanResult
{
    Memmy_Addr addr;
} Memmy_ScanResult;
```

```c
typedef struct Memmy_ScanResultNode
{
    struct Memmy_ScanResultNode *next;
    Memmy_ScanResult result;
} Memmy_ScanResultNode;

typedef struct Memmy_ScanResultList
{
    Memmy_ScanResultNode *first;
    Memmy_ScanResultNode *last;
    U64 count;
} Memmy_ScanResultList;
```

### 14.3 Value Scan

```c
Memmy_Status Memmy_Process_ScanValue(Arena *arena,
                                 Memmy_Process *process,
                                 Memmy_RegionList *regions,
                                 Memmy_ScanOptions *options,
                                 Memmy_Type type,
                                 void *value,
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

If `--name` matches multiple processes, the command must fail with an ambiguity error and list matching PIDs.

### 15.2 Command: `procs`

```txt
memmy procs
memmy procs <filter>
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

### 15.3 Command: `mods`

```txt
memmy mods -p 1234
memmy mods -p 1234 kernel32
memmy mods -p 1234 --json
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

### 15.4 Command: `addr`

```txt
memmy addr -p 1234 'client.dll+0x4242->0x123->0x4'
memmy addr -p 1234 'client.dll+0x4242->0x123->0x4' --json
```

Text output:

```txt
client.dll base: 0x00007ff800000000
+ 0x4242        = 0x00007ff800004242
deref           = 0x000001d812340000
+ 0x123         = 0x000001d812340123
deref           = 0x000001d856780000
+ 0x4           = 0x000001d856780004

resolved: 0x000001d856780004
```

JSON output:

```json
{
  "address_expr": "client.dll+0x4242->0x123->0x4",
  "resolved_address": "0x000001d856780004",
  "trace": [
    {
      "kind": "module_base",
      "text": "client.dll",
      "output_address": "0x00007ff800000000"
    }
  ]
}
```

### 15.5 Command: `peek`

```txt
memmy peek -p 1234 <addr-expr> <type>
memmy peek -p 1234 <addr-expr> bytes <count>
memmy peek -p 1234 <addr-expr> str
memmy peek -p 1234 <addr-expr> wstr
```

Examples:

```txt
memmy peek -p 1234 'client.dll+0x4242' u32
memmy peek -p 1234 'client.dll+0x4242' bytes 64
memmy peek -p 1234 'client.dll+0x4242->0x8' ptr
```

Text output:

```txt
0x000001d856780004: u32 1337  0x00000539
```

JSON output:

```json
{
  "address_expr": "client.dll+0x4242",
  "resolved_address": "0x000001d856780004",
  "type": "u32",
  "value": 1337,
  "hex": "0x00000539"
}
```

### 15.6 Command: `poke`

```txt
memmy poke -p 1234 <addr-expr> <type> <value>
memmy poke -p 1234 <addr-expr> bytes '90 90 90'
memmy poke -p 1234 <addr-expr> str 'hello'
```

Examples:

```txt
memmy poke -p 1234 'client.dll+0x4242' u32 1337
memmy poke -p 1234 'client.dll+0x4242' bytes '90 90 90'
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

### 15.7 Command: `scan`

```txt
memmy scan -p 1234 <type> <value>
memmy scan -p 1234 --range <range-expr> <type> <value>
memmy scan -p 1234 --readable u32 1337
memmy scan -p 1234 --writable u32 1337
memmy scan -p 1234 --executable bytes '48 8B'
```

Examples:

```txt
memmy scan -p 1234 u32 1337
memmy scan -p 1234 --range client.dll u32 1337
memmy scan -p 1234 --range 'client.dll+0x1000..+0x5000' u32 1337
```

Text output:

```txt
ADDRESS             MODULE+OFFSET
0x00007ff800004242  client.dll+0x4242
0x00007ff800007abc  client.dll+0x7abc
```

JSONL output:

```json
{"address":"0x00007ff800004242","module":"client.dll","offset":"0x4242"}
{"address":"0x00007ff800007abc","module":"client.dll","offset":"0x7abc"}
```

### 15.8 Command: `pscan`

```txt
memmy pscan -p 1234 <pattern>
memmy pscan -p 1234 --range <range-expr> <pattern>
memmy pscan -p 1234 --executable <pattern>
```

Examples:

```txt
memmy pscan -p 1234 '48 8B ?? ?? 89'
memmy pscan -p 1234 --range client.dll '48 8B ?? ?? 89'
```

Output format matches `scan`.

---

## 16. Range Expressions

Range expressions are used by `scan` and `pscan`.

Valid examples:

```txt
client.dll
client.dll+0x1000..+0x5000
0x10000000..0x10010000
client.dll+0x1000..client.dll+0x5000
```

Initial required support:

```txt
module
address_expr..address_expr
module+start..+end
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
                                 Memmy_Range *out,
                                 Memmy_Error *error);
```

For a module-only range:

```txt
client.dll
```

the range resolves to:

```txt
module.base..module.base + module.size
```

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
0x00007ff800004242  client.dll+0x4242
```

Function:

```c
String8 Memmy_FormatAddress(Arena *arena,
                          Memmy_ModuleList *modules,
                          Memmy_Addr addr,
                          Memmy_PointerWidth pointer_width);
```

### 17.3 JSON Rules

JSON output should:

```txt
1. Use stable key names.
2. Format addresses as strings.
3. Format sizes as strings when hex is clearer.
4. Avoid pretty tables.
5. Avoid human-only prose.
```

Addresses in JSON are strings to avoid integer precision loss in JavaScript consumers.

---

## 18. Windows Backend

Windows is the first supported backend.

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

A fake backend should support:

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

Fake backend type:

```c
typedef struct Memmy_FakeProcess
{
    U8 *memory;
    U64 memory_size;
    Memmy_Addr base_addr;

    Memmy_ModuleList modules;
    Memmy_RegionList regions;

    Memmy_PointerWidth pointer_width;
} Memmy_FakeProcess;
```

---

## 22. Initial Milestones

### Milestone 1: Core CLI Skeleton

```txt
memmy --help
memmy procs
memmy mods -p <pid>
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
memmy peek -p <pid> <addr-expr> <type>
memmy addr -p <pid> <addr-expr>
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
memmy poke -p <pid> <addr-expr> <type> <value>
memmy poke --dry-run ...
```

Includes:

```txt
typed value parser
safe old/new display
write support
```

### Milestone 4: Pattern Scan

```txt
memmy pscan -p <pid> <pattern>
memmy pscan -p <pid> --range <range> <pattern>
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
memmy scan -p <pid> <type> <value>
memmy scan -p <pid> --range <range> <type> <value>
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
expression arithmetic beyond simple address expressions
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
11. Core parser/scanner logic should be testable with a fake backend.
12. Windows comes first, but the API shape must not be Windows-only.
```
