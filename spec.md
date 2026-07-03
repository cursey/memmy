# Memory Introspection Toolkit Specification

## 1. Overview

This project provides a native memory introspection toolkit for inspecting and modifying the virtual memory of local processes.

The primary user interface is a single executable with subcommands:

```txt
mem procs
mem mods  -p 1234
mem addr  -p 1234 'client.dll+0x4242->0x123->0x4'
mem peek  -p 1234 'client.dll+0x4242->0x123->0x4' u32
mem poke  -p 1234 'client.dll+0x4242->0x123->0x4' u32 1337
mem scan  -p 1234 --range client.dll u32 1337
mem pscan -p 1234 --range client.dll '48 8B ?? ?? 89'
```

The implementation is split into:

```txt
Base layer
  Primitive types, arenas, strings, arrays, formatting, utilities.

Mem core layer
  Process-agnostic memory model, address expression parser, pattern parser,
  scanner, type parser, command-independent data structures.

Platform layer
  OS-specific process enumeration, process opening, memory read/write,
  region enumeration, module enumeration.

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

System-specific types are prefixed with the system name:

```c
Mem_Process
Mem_Module
Mem_Region
Mem_AddressExpr
Mem_Pattern
Mem_ScanResult
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
typedef U32 Mem_Access;
enum
{
    Mem_Access_Read    = 1u << 0,
    Mem_Access_Write   = 1u << 1,
    Mem_Access_Execute = 1u << 2,
};
```

### 2.3 Functions

Functions use `PascalCase`.

System layer, dominant type:

```c
Mem_Process_Open
Mem_Process_Close
Mem_Process_Read
Mem_Process_Write
Mem_ModuleList_Find
Mem_AddressExpr_Parse
Mem_AddressExpr_Resolve
Mem_Pattern_Parse
Mem_Pattern_Match
```

System layer, no dominant type:

```c
Mem_ListProcesses
Mem_FormatAddress
Mem_ParseType
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
typedef struct Mem_Module
{
    String8 name;
    String8 path;
    U64 base;
    U64 size;
} Mem_Module;
```

### 2.5 Macros

Constant macros use `UPPER_CASE`:

```c
#define KB(n) ((U64)(n) << 10)
#define MB(n) ((U64)(n) << 20)
#define GB(n) ((U64)(n) << 30)

#define MAX_U32 0xffffffffu
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
mem/
  include/
    base/
      base_types.h
      base_arena.h
      base_string.h
      base_array.h
      base_format.h

    mem/
      mem_core.h
      mem_process.h
      mem_module.h
      mem_region.h
      mem_address_expr.h
      mem_pattern.h
      mem_scan.h
      mem_type.h
      mem_error.h
      mem_backend.h

    cli/
      cli_core.h
      cli_command.h

  src/
    base/
      base_arena.c
      base_string.c
      base_format.c

    mem/
      mem_address_expr.c
      mem_pattern.c
      mem_scan.c
      mem_type.c
      mem_format.c

    platform/
      win32/
        win32_process.c
        win32_memory.c
        win32_module.c
        win32_region.c

      linux/
        linux_process.c
        linux_memory.c
        linux_module.c
        linux_region.c

      macos/
        macos_process.c
        macos_memory.c
        macos_module.c
        macos_region.c

    cli/
      cli_main.c
      cli_help.c
      cli_json.c
      cmd_addr.c
      cmd_peek.c
      cmd_poke.c
      cmd_scan.c
      cmd_pscan.c
      cmd_mods.c
      cmd_procs.c

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
typedef U64 Mem_Addr;
typedef U64 Mem_Size;
```

A remote address must not be represented as `void *` outside of the platform layer.

Correct:

```c
Mem_Addr addr;
```

Incorrect:

```c
void *remote_addr;
```

Platform backends are responsible for casting `Mem_Addr` to the required OS-specific pointer type at the boundary.

---

### 4.2 Process Handle

The public process type is opaque.

```c
typedef struct Mem_Process Mem_Process;
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
typedef U32 Mem_PointerWidth;
enum
{
    Mem_PointerWidth_Unknown,
    Mem_PointerWidth_32,
    Mem_PointerWidth_64,
};
```

Pointer reads in address expression resolution use the target process pointer width, not the host pointer width.

---

### 4.4 Endianness

Initial support assumes little-endian targets.

```c
typedef U32 Mem_Endian;
enum
{
    Mem_Endian_Little,
    Mem_Endian_Big,
};
```

Big-endian support is not required for the first implementation.

---

## 5. Error Model

Errors are explicit and non-global.

```c
typedef U32 Mem_Status;
enum
{
    Mem_Status_Ok,

    Mem_Status_InvalidArgument,
    Mem_Status_NotFound,
    Mem_Status_AccessDenied,
    Mem_Status_PartialRead,
    Mem_Status_PartialWrite,
    Mem_Status_Unreadable,
    Mem_Status_Unwritable,
    Mem_Status_ParseError,
    Mem_Status_Unsupported,
    Mem_Status_PlatformError,
    Mem_Status_OutOfMemory,
};
```

```c
typedef struct Mem_Error
{
    Mem_Status status;
    U32 os_code;
    String8 message;
} Mem_Error;
```

Most fallible functions return `Mem_Status` and optionally fill `Mem_Error`.

```c
Mem_Status Mem_Process_Read(Mem_Process *process,
                            Mem_Addr addr,
                            void *buffer,
                            U64 size,
                            U64 *bytes_read,
                            Mem_Error *error);
```

Guidelines:

* `Mem_Status_Ok` means the operation fully succeeded.
* `Mem_Status_PartialRead` means at least one byte was read, but less than requested.
* `Mem_Status_Unreadable` means the address range could not be read.
* `Mem_Status_PlatformError` means an OS-specific failure occurred and `os_code` should be set.

---

## 6. Platform Backend

The platform backend abstracts OS-specific process and memory operations.

```c
typedef struct Mem_Backend Mem_Backend;

typedef struct Mem_Backend
{
    String8 name;

    U32 capabilities;

    Mem_Status (*list_processes)(Arena *arena,
                                 Mem_ProcessList *out,
                                 Mem_Error *error);

    Mem_Status (*open_process)(U32 pid,
                               Mem_ProcessAccess access,
                               Mem_Process **out,
                               Mem_Error *error);

    void (*close_process)(Mem_Process *process);

    Mem_Status (*read)(Mem_Process *process,
                       Mem_Addr addr,
                       void *buffer,
                       U64 size,
                       U64 *bytes_read,
                       Mem_Error *error);

    Mem_Status (*write)(Mem_Process *process,
                        Mem_Addr addr,
                        void *buffer,
                        U64 size,
                        U64 *bytes_written,
                        Mem_Error *error);

    Mem_Status (*list_modules)(Arena *arena,
                               Mem_Process *process,
                               Mem_ModuleList *out,
                               Mem_Error *error);

    Mem_Status (*list_regions)(Arena *arena,
                               Mem_Process *process,
                               Mem_RegionList *out,
                               Mem_Error *error);
} Mem_Backend;
```

### 6.1 Backend Capabilities

```c
typedef U32 Mem_BackendCap;
enum
{
    Mem_BackendCap_Read        = 1u << 0,
    Mem_BackendCap_Write       = 1u << 1,
    Mem_BackendCap_ListProcs   = 1u << 2,
    Mem_BackendCap_ListModules = 1u << 3,
    Mem_BackendCap_ListRegions = 1u << 4,
    Mem_BackendCap_Protect     = 1u << 5,
};
```

The CLI should produce clear errors when a command requires an unsupported capability.

---

## 7. Processes

### 7.1 Process Info

```c
typedef struct Mem_ProcessInfo
{
    U32 pid;
    String8 name;
    String8 path;
    Mem_PointerWidth pointer_width;
} Mem_ProcessInfo;
```

```c
typedef struct Mem_ProcessNode
{
    struct Mem_ProcessNode *next;
    Mem_ProcessInfo info;
} Mem_ProcessNode;

typedef struct Mem_ProcessList
{
    Mem_ProcessNode *first;
    Mem_ProcessNode *last;
    U64 count;
} Mem_ProcessList;
```

### 7.2 Process Access

```c
typedef U32 Mem_ProcessAccess;
enum
{
    Mem_ProcessAccess_Read  = 1u << 0,
    Mem_ProcessAccess_Write = 1u << 1,
    Mem_ProcessAccess_Query = 1u << 2,
};
```

### 7.3 Required Functions

```c
Mem_Status Mem_ListProcesses(Arena *arena,
                             Mem_ProcessList *out,
                             Mem_Error *error);

Mem_Status Mem_Process_Open(U32 pid,
                            Mem_ProcessAccess access,
                            Mem_Process **out,
                            Mem_Error *error);

void Mem_Process_Close(Mem_Process *process);
```

---

## 8. Modules

### 8.1 Module Type

```c
typedef struct Mem_Module
{
    String8 name;
    String8 path;
    Mem_Addr base;
    Mem_Size size;
} Mem_Module;
```

```c
typedef struct Mem_ModuleNode
{
    struct Mem_ModuleNode *next;
    Mem_Module module;
} Mem_ModuleNode;

typedef struct Mem_ModuleList
{
    Mem_ModuleNode *first;
    Mem_ModuleNode *last;
    U64 count;
} Mem_ModuleList;
```

### 8.2 Required Functions

```c
Mem_Status Mem_Process_ListModules(Arena *arena,
                                   Mem_Process *process,
                                   Mem_ModuleList *out,
                                   Mem_Error *error);

Mem_Module *Mem_ModuleList_FindByName(Mem_ModuleList *modules,
                                      String8 name);

Mem_Module *Mem_ModuleList_FindByAddress(Mem_ModuleList *modules,
                                         Mem_Addr addr);
```

Module name lookup is case-insensitive on Windows. Linux and macOS behavior should be defined by the backend, but the CLI should generally try to be forgiving.

---

## 9. Memory Regions

### 9.1 Region Type

```c
typedef U32 Mem_RegionAccess;
enum
{
    Mem_RegionAccess_Read    = 1u << 0,
    Mem_RegionAccess_Write   = 1u << 1,
    Mem_RegionAccess_Execute = 1u << 2,
    Mem_RegionAccess_Guard   = 1u << 3,
};
```

```c
typedef U32 Mem_RegionState;
enum
{
    Mem_RegionState_Unknown,
    Mem_RegionState_Free,
    Mem_RegionState_Reserved,
    Mem_RegionState_Committed,
};
```

```c
typedef struct Mem_Region
{
    Mem_Addr base;
    Mem_Size size;
    Mem_RegionAccess access;
    Mem_RegionState state;
} Mem_Region;
```

```c
typedef struct Mem_RegionNode
{
    struct Mem_RegionNode *next;
    Mem_Region region;
} Mem_RegionNode;

typedef struct Mem_RegionList
{
    Mem_RegionNode *first;
    Mem_RegionNode *last;
    U64 count;
} Mem_RegionList;
```

### 9.2 Required Functions

```c
Mem_Status Mem_Process_ListRegions(Arena *arena,
                                   Mem_Process *process,
                                   Mem_RegionList *out,
                                   Mem_Error *error);

B32 Mem_Region_IsReadable(Mem_Region *region);
B32 Mem_Region_IsWritable(Mem_Region *region);
B32 Mem_Region_IsExecutable(Mem_Region *region);
```

---

## 10. Reading and Writing Memory

### 10.1 Read

```c
Mem_Status Mem_Process_Read(Mem_Process *process,
                            Mem_Addr addr,
                            void *buffer,
                            U64 size,
                            U64 *bytes_read,
                            Mem_Error *error);
```

### 10.2 Write

```c
Mem_Status Mem_Process_Write(Mem_Process *process,
                             Mem_Addr addr,
                             void *buffer,
                             U64 size,
                             U64 *bytes_written,
                             Mem_Error *error);
```

### 10.3 Pointer Read

Pointer reads are used by the address expression resolver.

```c
Mem_Status Mem_Process_ReadPtr(Mem_Process *process,
                               Mem_Addr addr,
                               Mem_PointerWidth pointer_width,
                               Mem_Addr *out,
                               Mem_Error *error);
```

For `Mem_PointerWidth_32`, this reads `U32` and zero-extends to `Mem_Addr`.

For `Mem_PointerWidth_64`, this reads `U64`.

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
typedef U32 Mem_AddressExprBaseKind;
enum
{
    Mem_AddressExprBaseKind_Address,
    Mem_AddressExprBaseKind_Module,
};
```

```c
typedef U32 Mem_AddressExprOpKind;
enum
{
    Mem_AddressExprOpKind_Add,
    Mem_AddressExprOpKind_Sub,
    Mem_AddressExprOpKind_Deref,
    Mem_AddressExprOpKind_DerefAdd,
};
```

```c
typedef struct Mem_AddressExprOp
{
    Mem_AddressExprOpKind kind;
    U64 value;
} Mem_AddressExprOp;
```

```c
typedef struct Mem_AddressExpr
{
    Mem_AddressExprBaseKind base_kind;

    Mem_Addr base_addr;
    String8 module_name;

    Mem_AddressExprOp *ops;
    U64 op_count;
} Mem_AddressExpr;
```

### 11.4 Resolve Trace

The resolver may optionally produce a trace.

```c
typedef U32 Mem_AddressTraceStepKind;
enum
{
    Mem_AddressTraceStepKind_BaseAddress,
    Mem_AddressTraceStepKind_ModuleBase,
    Mem_AddressTraceStepKind_Add,
    Mem_AddressTraceStepKind_Sub,
    Mem_AddressTraceStepKind_Deref,
    Mem_AddressTraceStepKind_DerefAdd,
};
```

```c
typedef struct Mem_AddressTraceStep
{
    Mem_AddressTraceStepKind kind;
    String8 text;
    Mem_Addr input_addr;
    Mem_Addr output_addr;
    U64 value;
} Mem_AddressTraceStep;
```

```c
typedef struct Mem_AddressTrace
{
    Mem_AddressTraceStep *steps;
    U64 step_count;
} Mem_AddressTrace;
```

### 11.5 Required Functions

```c
Mem_Status Mem_AddressExpr_Parse(Arena *arena,
                                 String8 text,
                                 Mem_AddressExpr *out,
                                 Mem_Error *error);

Mem_Status Mem_AddressExpr_Resolve(Arena *arena,
                                   Mem_Process *process,
                                   Mem_ModuleList *modules,
                                   Mem_PointerWidth pointer_width,
                                   Mem_AddressExpr *expr,
                                   Mem_Addr *out,
                                   Mem_AddressTrace *trace,
                                   Mem_Error *error);
```

If `trace` is null, no trace is produced.

---

## 12. Data Types

Typed reads, writes, and scans use a shared type system.

```c
typedef U32 Mem_TypeKind;
enum
{
    Mem_TypeKind_U8,
    Mem_TypeKind_U16,
    Mem_TypeKind_U32,
    Mem_TypeKind_U64,

    Mem_TypeKind_I8,
    Mem_TypeKind_I16,
    Mem_TypeKind_I32,
    Mem_TypeKind_I64,

    Mem_TypeKind_F32,
    Mem_TypeKind_F64,

    Mem_TypeKind_Ptr,

    Mem_TypeKind_Bytes,
    Mem_TypeKind_Str,
    Mem_TypeKind_WStr,
};
```

```c
typedef struct Mem_Type
{
    Mem_TypeKind kind;
    U64 size;
} Mem_Type;
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
Mem_Status Mem_Type_Parse(String8 text,
                          Mem_Type *out,
                          Mem_Error *error);

U64 Mem_Type_Size(Mem_Type type,
                  Mem_PointerWidth pointer_width);
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
typedef struct Mem_PatternByte
{
    U8 value;
    U8 mask;
} Mem_PatternByte;
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
typedef struct Mem_Pattern
{
    Mem_PatternByte *bytes;
    U64 count;
} Mem_Pattern;
```

### 13.3 Required Functions

```c
Mem_Status Mem_Pattern_Parse(Arena *arena,
                             String8 text,
                             Mem_Pattern *out,
                             Mem_Error *error);

B32 Mem_Pattern_Match(Mem_Pattern *pattern,
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
typedef U32 Mem_ScanFlags;
enum
{
    Mem_ScanFlag_ReadableOnly    = 1u << 0,
    Mem_ScanFlag_WritableOnly    = 1u << 1,
    Mem_ScanFlag_ExecutableOnly  = 1u << 2,
};
```

```c
typedef struct Mem_Range
{
    Mem_Addr base;
    Mem_Size size;
} Mem_Range;
```

```c
typedef struct Mem_ScanOptions
{
    Mem_Range *ranges;
    U64 range_count;

    Mem_ScanFlags flags;

    U64 chunk_size;
    U64 max_results;
} Mem_ScanOptions;
```

### 14.2 Scan Result

```c
typedef struct Mem_ScanResult
{
    Mem_Addr addr;
} Mem_ScanResult;
```

```c
typedef struct Mem_ScanResultNode
{
    struct Mem_ScanResultNode *next;
    Mem_ScanResult result;
} Mem_ScanResultNode;

typedef struct Mem_ScanResultList
{
    Mem_ScanResultNode *first;
    Mem_ScanResultNode *last;
    U64 count;
} Mem_ScanResultList;
```

### 14.3 Value Scan

```c
Mem_Status Mem_Process_ScanValue(Arena *arena,
                                 Mem_Process *process,
                                 Mem_RegionList *regions,
                                 Mem_ScanOptions *options,
                                 Mem_Type type,
                                 void *value,
                                 Mem_ScanResultList *out,
                                 Mem_Error *error);
```

### 14.4 Pattern Scan

```c
Mem_Status Mem_Process_ScanPattern(Arena *arena,
                                   Mem_Process *process,
                                   Mem_RegionList *regions,
                                   Mem_ScanOptions *options,
                                   Mem_Pattern *pattern,
                                   Mem_ScanResultList *out,
                                   Mem_Error *error);
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
#define MEM_DEFAULT_SCAN_CHUNK_SIZE MB(16)
```

---

## 15. CLI

The CLI executable is named:

```txt
mem
```

The general shape is:

```txt
mem [global-options] <command> [command-options]
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
mem procs
mem procs <filter>
mem procs --json
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
mem mods -p 1234
mem mods -p 1234 kernel32
mem mods -p 1234 --json
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
mem addr -p 1234 'client.dll+0x4242->0x123->0x4'
mem addr -p 1234 'client.dll+0x4242->0x123->0x4' --json
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
mem peek -p 1234 <addr-expr> <type>
mem peek -p 1234 <addr-expr> bytes <count>
mem peek -p 1234 <addr-expr> str
mem peek -p 1234 <addr-expr> wstr
```

Examples:

```txt
mem peek -p 1234 'client.dll+0x4242' u32
mem peek -p 1234 'client.dll+0x4242' bytes 64
mem peek -p 1234 'client.dll+0x4242->0x8' ptr
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
mem poke -p 1234 <addr-expr> <type> <value>
mem poke -p 1234 <addr-expr> bytes '90 90 90'
mem poke -p 1234 <addr-expr> str 'hello'
```

Examples:

```txt
mem poke -p 1234 'client.dll+0x4242' u32 1337
mem poke -p 1234 'client.dll+0x4242' bytes '90 90 90'
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
mem scan -p 1234 <type> <value>
mem scan -p 1234 --range <range-expr> <type> <value>
mem scan -p 1234 --readable u32 1337
mem scan -p 1234 --writable u32 1337
mem scan -p 1234 --executable bytes '48 8B'
```

Examples:

```txt
mem scan -p 1234 u32 1337
mem scan -p 1234 --range client.dll u32 1337
mem scan -p 1234 --range 'client.dll+0x1000..+0x5000' u32 1337
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
mem pscan -p 1234 <pattern>
mem pscan -p 1234 --range <range-expr> <pattern>
mem pscan -p 1234 --executable <pattern>
```

Examples:

```txt
mem pscan -p 1234 '48 8B ?? ?? 89'
mem pscan -p 1234 --range client.dll '48 8B ?? ?? 89'
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
typedef struct Mem_RangeExpr
{
    String8 text;
} Mem_RangeExpr;
```

Required function:

```c
Mem_Status Mem_RangeExpr_Resolve(Arena *arena,
                                 Mem_Process *process,
                                 Mem_ModuleList *modules,
                                 Mem_PointerWidth pointer_width,
                                 String8 text,
                                 Mem_Range *out,
                                 Mem_Error *error);
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
String8 Mem_FormatAddress(Arena *arena,
                          Mem_ModuleList *modules,
                          Mem_Addr addr,
                          Mem_PointerWidth pointer_width);
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

`Mem_ProcessAccess_Read` requires:

```txt
PROCESS_VM_READ
PROCESS_QUERY_INFORMATION
```

`Mem_ProcessAccess_Write` requires:

```txt
PROCESS_VM_WRITE
PROCESS_VM_OPERATION
PROCESS_QUERY_INFORMATION
```

`Mem_ProcessAccess_Query` requires:

```txt
PROCESS_QUERY_INFORMATION
PROCESS_QUERY_LIMITED_INFORMATION
```

### 18.3 Region Mapping

Windows protection flags are mapped to `Mem_RegionAccess`.

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

The Linux backend should implement the same `Mem_Backend` interface.

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
typedef struct Mem_FakeProcess
{
    U8 *memory;
    U64 memory_size;
    Mem_Addr base_addr;

    Mem_ModuleList modules;
    Mem_RegionList regions;

    Mem_PointerWidth pointer_width;
} Mem_FakeProcess;
```

---

## 22. Initial Milestones

### Milestone 1: Core CLI Skeleton

```txt
mem --help
mem procs
mem mods -p <pid>
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
mem peek -p <pid> <addr-expr> <type>
mem addr -p <pid> <addr-expr>
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
mem poke -p <pid> <addr-expr> <type> <value>
mem poke --dry-run ...
```

Includes:

```txt
typed value parser
safe old/new display
write support
```

### Milestone 4: Pattern Scan

```txt
mem pscan -p <pid> <pattern>
mem pscan -p <pid> --range <range> <pattern>
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
mem scan -p <pid> <type> <value>
mem scan -p <pid> --range <range> <type> <value>
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
3. Platform specifics stay behind Mem_Backend.
4. The CLI must be pleasant for humans.
5. JSON/JSONL must be stable for agents.
6. Scans should minimize remote reads.
7. Help text should teach the address expression language.
8. Dangerous operations should support dry-run or explain modes.
9. Core parser/scanner logic should be testable with a fake backend.
10. Windows comes first, but the API shape must not be Windows-only.
```
