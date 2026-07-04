# Memmy v1 Specification

## 1. Scope

Memmy v1 is a small native CLI and C library for inspecting and modifying
memory in local processes.

The current repository implements the v0 command model: explicit process,
module, region, peek, poke, scan, and pattern-scan commands over numeric
addresses and ranges. It is already split into a reusable core library
(`memmy`), a CLI library (`memmy_cli`), and a thin executable (`cmd_memmy`).

v1 keeps that model and adds two new libraries: `memmy_expr` for memory
expression parsing and `memmy_exec` for resolving and executing parsed
expressions. Top-level `--expr` in `memmy_cli` should be a thin dispatcher over
those libraries. The main goal is to support absolute addresses,
module-relative addresses, pointer chains, process-qualified targets,
module-relative ranges, whole-process ranges, typed peeks, typed pokes, pattern
scans, and exact value scans through one expression grammar.

Primary CLI:

```txt
memmy procs
memmy mods  --pid 1234
memmy regions --pid 1234

memmy peek  --pid 1234 --addr 0x000001d856780004 --type u32
memmy poke  --pid 1234 --addr 0x000001d856780004 --type u32 --value 1337
memmy scan  --pid 1234 --start 0x00007ff800000000 --end 0x00007ff8001a4000 --type u32 --value 1337
memmy pscan --pid 1234 --start 0x00007ff800000000 --length 0x1a4000 --pattern "48 8B ?? ?? 89"

memmy --pid 1234 --expr "0x000001d856780004 : u32"
memmy --expr "<game.exe!client.dll>+0x123 : i32"
memmy --expr "<game.exe!client.dll>+0x123 : i32 = 77"
memmy --expr "<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}"
memmy --expr "<game.exe!> : i32 == 42"
```

The v0 absolute-address and absolute-range subcommands remain canonical
explicit forms for scripts. Top-level `--expr` is the only expression entry
point.

Windows is the first backend. Linux and macOS are future targets.

## 2. Explicit Non-Goals

v1 does not include:

```txt
REPL
general scripting
RHS expression evaluation for writes
copying values between addresses
ordering comparisons in value scans
array views
typed value constructors
address_expr..address_expr ranges
quoted process or module names inside target refs
symbol loading
PDB/DWARF support
repeated narrowing scans
remote machine support
kernel memory support
GUI
disassembly
remote allocation/free
memory protection changes
thread enumeration or suspension
```

The only scan comparison operator required by v1 is `==`. Other comparison
operators are reserved for future versions.

## 3. Project Shape

Current repository shape:

```txt
base/
  Shared foundation library. Target: base.

memmy/
  Core Memmy library. Target: memmy. Public headers live in memmy/include.
  Owns process, backend, numeric range, value, pattern, and scan APIs.
  Platform backend code lives under memmy/src/platform/<os>/.

memmy_expr/
  Memory expression library. Target: memmy_expr. Public headers live in
  memmy_expr/include. Owns expression parsing and expression data structures
  only. It does not enumerate processes, open processes, read memory, write
  memory, scan memory, or format output.

memmy_exec/
  Expression execution library. Target: memmy_exec. Public headers live in
  memmy_exec/include. Owns process-relative target/address/range resolution and
  execution of parsed expressions over the core memmy APIs. It is independent
  of CLI argv parsing and output formatting.

memmy_cli/
  CLI library. Target: memmy_cli. This owns command-line parsing, command
  dispatch, process selection policy, text/JSON/JSONL formatting, and
  CLI-facing helpers. The executable links this library instead of implementing
  command behavior directly.

cmd/memmy/
  Thin CLI executable. Target: cmd_memmy. Output name: memmy.

unittest/
  Test executable and test framework. Target: memmy_test. Tests are split by
  subsystem under unittest/base and unittest/memmy, including separate CLI
  command tests and core-library tests.

vendor/
  Third-party code.
```

Top-level CMake adds these subdirectories in dependency order:

```txt
vendor
base
memmy
memmy_expr
memmy_exec
memmy_cli
cmd
unittest
```

v1 should preserve this split:

```txt
memmy       owns reusable process, memory, value, scan, and range APIs
memmy_expr  owns reusable expression parsing and parsed expression structures
memmy_exec  owns reusable expression resolution and execution APIs
memmy_cli   owns CLI option parsing, top-level --expr dispatch, and output formatting
cmd_memmy   remains a small main() wrapper around memmy_cli
```

Expression parsing and target-independent expression data structures belong in
`memmy_expr`. Process-relative resolution and execution helpers belong in
`memmy_exec`. Process selection from `--pid`, `--name`, or process-qualified
expression targets belongs in `memmy_cli`, because it requires process
enumeration and opening policy. `memmy_expr` may define selector data
structures, but it should not enumerate or open processes as part of parsing.

## 4. Naming and Style

Follow the repository rules in `AGENTS.md`.

Important v1 names:

```c
typedef U64 Memmy_Addr;
typedef U64 Memmy_Size;
```

Remote process addresses are integers, not local pointers, outside platform
backend code.

Public project APIs use `Memmy_` names. Functions use `PascalCase`. Variables
and fields use `snake_case`. Allocation goes through `Arena`.

## 5. Build and Test

Configure and build:

```txt
cmake -S . -B build
cmake --build build
```

Current and v1 targets:

```txt
base        shared foundation library, current
memmy       core library, current
memmy_expr  memory expression library, new in v1
memmy_exec  expression execution library, new in v1
memmy_cli   CLI library, current
cmd_memmy   executable target, output name memmy, current
memmy_test  unit test executable, current
```

On multi-config Windows generators, the CLI executable is emitted under:

```txt
build/cmd/memmy/<config>/memmy.exe
```

Tests are built into `memmy_test`. CTest cases are discovered after
`memmy_test` builds through `unittest/discover_memmy_tests.cmake`, which emits
CTest include files in the build tree. Run tests with:

```txt
ctest --test-dir build
```

## 6. Address and Size Input

The v0 numeric address and size parsers remain available. An address or size
input is one unsigned integer token.

Accepted:

```txt
0x000001d856780004
0X1000
4096
```

Rejected:

```txt
-1
+1
0x1000+4
(0x1000)
client.dll
```

Hex input uses `0x` or `0X`. Decimal input has no prefix. Overflow returns
`Memmy_Status_Overflow`. Invalid syntax returns `Memmy_Status_ParseError`.

Required helpers:

```c
Memmy_Status Memmy_ParseAddress(String8 text,
                                Memmy_Addr *out,
                                Memmy_Error *error);

Memmy_Status Memmy_ParseSize(String8 text,
                             Memmy_Size *out,
                             Memmy_Error *error);
```

## 7. Ranges

A range is one half-open interval:

```txt
[start, end)
```

CLI range input keeps the v0 forms:

```txt
--start <addr> --end <addr>
--start <addr> --length <size>
```

Rules:

```txt
start <= end
end = start + length
start + length must not overflow
length may be zero
zero-length ranges produce no scan results
```

Type:

```c
typedef struct Memmy_Range
{
    Memmy_Addr start;
    Memmy_Addr end;
} Memmy_Range;
```

Required helpers:

```c
Memmy_Status Memmy_Range_FromStartEnd(Memmy_Addr start,
                                      Memmy_Addr end,
                                      Memmy_Range *out,
                                      Memmy_Error *error);

Memmy_Status Memmy_Range_FromStartLength(Memmy_Addr start,
                                         Memmy_Size length,
                                         Memmy_Range *out,
                                         Memmy_Error *error);
```

## 8. Error Model

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

Initial error contexts:

```txt
address
range
type
value
pattern
expr
target
backend
cli
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

JSON failures use this shape:

```json
{
  "ok": false,
  "error": {
    "status": "parse_error",
    "message": "expected hexadecimal digit",
    "context": "address",
    "input": "0x",
    "byte_offset": 2,
    "byte_count": 1,
    "os_code": 0
  }
}
```

## 9. Backend Boundary

Platform SDK headers and native OS calls belong under
`memmy/src/platform/<os>/` only.

```c
typedef struct Memmy_Context
{
    Memmy_Backend *backend;
} Memmy_Context;
```

```c
typedef struct Memmy_Backend Memmy_Backend;
struct Memmy_Backend
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
};
```

Capabilities:

```c
typedef U32 Memmy_BackendCap;
enum
{
    Memmy_BackendCap_Read        = 1u << 0,
    Memmy_BackendCap_Write       = 1u << 1,
    Memmy_BackendCap_ListProcs   = 1u << 2,
    Memmy_BackendCap_ListModules = 1u << 3,
    Memmy_BackendCap_ListRegions = 1u << 4,
};
```

## 10. Process, Module, and Region Types

```c
typedef U32 Memmy_PointerWidth;
enum
{
    Memmy_PointerWidth_Unknown,
    Memmy_PointerWidth_32,
    Memmy_PointerWidth_64,
};
```

```c
typedef U32 Memmy_ProcessAccess;
enum
{
    Memmy_ProcessAccess_Read  = 1u << 0,
    Memmy_ProcessAccess_Write = 1u << 1,
    Memmy_ProcessAccess_Query = 1u << 2,
};
```

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

```c
typedef struct Memmy_ProcessInfo
{
    ListLink link;
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
} Memmy_ProcessInfo;

typedef struct Memmy_ProcessList
{
    List list; // Memmy_ProcessInfo
} Memmy_ProcessList;
```

```c
typedef struct Memmy_Module
{
    ListLink link;
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
} Memmy_Module;

typedef struct Memmy_ModuleList
{
    List list; // Memmy_Module
} Memmy_ModuleList;
```

```c
typedef U32 Memmy_RegionAccess;
enum
{
    Memmy_RegionAccess_Read    = 1u << 0,
    Memmy_RegionAccess_Write   = 1u << 1,
    Memmy_RegionAccess_Execute = 1u << 2,
    Memmy_RegionAccess_Guard   = 1u << 3,
};

typedef U32 Memmy_RegionState;
enum
{
    Memmy_RegionState_Free,
    Memmy_RegionState_Reserved,
    Memmy_RegionState_Committed,
};

typedef struct Memmy_Region
{
    ListLink link;
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
} Memmy_Region;

typedef struct Memmy_RegionList
{
    List list; // Memmy_Region
} Memmy_RegionList;
```

Required APIs:

```c
Memmy_Status Memmy_ListProcesses(Arena *arena,
                                 Memmy_ProcessList *out,
                                 Memmy_Error *error);

Memmy_Status Memmy_Process_Open(Arena *arena,
                                U32 pid,
                                Memmy_ProcessAccess access,
                                Memmy_Process **out,
                                Memmy_Error *error);

B32 Memmy_Process_IsOpen(Memmy_Process *process);
void Memmy_Process_Close(Memmy_Process *process);

Memmy_Status Memmy_Process_ListModules(Arena *arena,
                                       Memmy_Process *process,
                                       Memmy_ModuleList *out,
                                       Memmy_Error *error);

Memmy_Status Memmy_Process_ListRegions(Arena *arena,
                                       Memmy_Process *process,
                                       Memmy_RegionList *out,
                                       Memmy_Error *error);

Memmy_Status Memmy_Process_Read(Memmy_Process *process,
                                Memmy_Addr addr,
                                void *buffer,
                                U64 size,
                                U64 *bytes_read,
                                Memmy_Error *error);

Memmy_Status Memmy_Process_Write(Memmy_Process *process,
                                 Memmy_Addr addr,
                                 void *buffer,
                                 U64 size,
                                 U64 *bytes_written,
                                 Memmy_Error *error);
```

## 11. Types, Values, and Patterns

Required value types:

```txt
u8   i8
u16  i16
u32  i32
u64  i64
f32  f64
ptr
bytes
str
wstr
```

`ptr` uses the target process pointer width. `bytes`, `str`, and `wstr` are
variable-width values.

`fixed_size` is the byte width known from the parsed type alone. It is `0` for
variable-width types and for `ptr` before target pointer width is applied.

```c
typedef U32 Memmy_TypeKind;

typedef struct Memmy_Type
{
    Memmy_TypeKind kind;
    U64 fixed_size;
} Memmy_Type;

typedef struct Memmy_Value
{
    Memmy_Type type;
    String8 bytes;
} Memmy_Value;
```

```c
Memmy_Status Memmy_Type_Parse(String8 text,
                              Memmy_Type *out,
                              Memmy_Error *error);

Memmy_Status Memmy_Value_Parse(Arena *arena,
                               Memmy_Type type,
                               Memmy_PointerWidth pointer_width,
                               String8 text,
                               Memmy_Value *out,
                               Memmy_Error *error);
```

Patterns are byte sequences. Wildcards are accepted only when
`Memmy_PatternParseFlag_AllowWildcards` is set.

```txt
48 8B ?? ?? 89
```

```c
typedef struct Memmy_PatternByte
{
    U8 value;
    B32 wildcard;
} Memmy_PatternByte;

typedef struct Memmy_Pattern
{
    Memmy_PatternByte *bytes;
    U64 count;
} Memmy_Pattern;

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
```

`pscan --pattern` and expression pattern scans pass
`Memmy_PatternParseFlag_AllowWildcards`. Parsing a `bytes` value for `peek`,
`poke`, or `scan` rejects wildcards.

## 12. Memory Expressions

Memory expressions are the primary v1 addition. They are parsed independently
from command execution so that the CLI can dispatch top-level `memmy --expr`.

Required examples:

```txt
0x000001d856780004 : u32
<client.dll>+0x123
<client.dll>+0x123->0x8
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : i32 = 77
<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}
<game.exe!> : i32 == 42
```

The unqualified module examples are process-relative. They are valid when the
caller already has a `Memmy_Process *` or when top-level `memmy --expr` is
paired with `--pid` or `--name`.

### 12.1 Expression Kinds

```c
typedef U32 Memmy_MemoryExprKind;
enum
{
    Memmy_MemoryExprKind_Address,
    Memmy_MemoryExprKind_Peek,
    Memmy_MemoryExprKind_Poke,
    Memmy_MemoryExprKind_PatternScan,
    Memmy_MemoryExprKind_ValueScan,
};
```

Dispatch by expression shape:

```txt
address_expr                 resolve one address
address_expr : type          peek one typed value
address_expr : type = value  poke one typed value
range_expr { pattern }       pattern scan
range_expr : type == value   exact value scan
```

The only v1 value-scan operator is `==`.

### 12.2 Target References

Targets identify either a module in the selected process or the whole selected
process.

```txt
target_ref :=
    '<' module_name '>'
  | '<' process_selector '!' module_name '>'
  | '<' process_selector '!' '>'

process_selector :=
    pid
  | process_name
```

Examples:

```txt
<client.dll>
<game.exe!client.dll>
<123!client.dll>
<game.exe!>
<123!>
```

Rules:

```txt
module_name and process_name are non-empty
module_name and process_name must not contain '<', '>', or '!'
leading or trailing whitespace inside target refs is invalid
<123> is a module name, not a PID target
<123!> is a whole-process target selected by PID
<123!client.dll> is a module target selected by PID
```

An unqualified module target such as `<client.dll>` is process-relative and
requires the caller to supply an already-open `Memmy_Process *` when resolving
or executing it. In the CLI, top-level `memmy --expr` supplies that process with
`--pid` or `--name`.

A qualified target such as `<game.exe!client.dll>` or `<123!client.dll>` selects
the process inside the expression. Supplying both an external process selector
and an expression process selector is invalid unless they refer to the same PID.

Process-name selection uses the same semantics as CLI `--name`: exactly one
matching process is required. Multiple matches return `Memmy_Status_Ambiguous`.
No matches return `Memmy_Status_NotFound`.

### 12.3 Address Expressions

Address expressions resolve one remote address.

```txt
address_expr := address_base address_op*

address_base := integer
              | target_ref

address_op   := add
              | sub
              | deref
              | deref_offset

add          := '+' offset
sub          := '-' offset
deref        := '->'
deref_offset := '->' offset

offset       := integer
              | '(' const_expr ')'
```

Semantics:

```txt
0x000001d856780004
```

means the absolute remote address `0x000001d856780004` in the selected process.

```txt
<client.dll>+0x123
```

means:

```txt
module_base("client.dll") + 0x123
```

```txt
addr->offset
```

means:

```txt
ReadPtr(addr) + offset
```

```txt
addr->
```

means:

```txt
ReadPtr(addr)
```

Pointer reads use the target process pointer width. For 32-bit targets,
`ReadPtr` reads `U32` and zero-extends to `Memmy_Addr`. For 64-bit targets,
`ReadPtr` reads `U64`.

Absolute address expressions require a selected process for any operation that
reads, writes, scans, or dereferences through remote memory. Top-level
`memmy --expr` supplies that process with `--pid` or `--name`.

### 12.4 Constant Expressions

Constant expressions are used only inside parentheses and module-relative
range brackets.

```txt
const_expr      := sum
sum             := product (('+' | '-') product)*
product         := unary (('*' | '/' | '%') unary)*
unary           := ('+' | '-') unary
                 | integer
                 | '(' const_expr ')'

integer         := hex_integer
                 | decimal_integer

hex_integer     := ('0x' | '0X') [0-9a-fA-F]+
decimal_integer := [0-9]+
```

Constant expressions use standard arithmetic precedence: parentheses, unary
`+`/`-`, `*`/`/`/`%`, then `+`/`-`, with binary operators associating
left-to-right.

Integer literal overflow, division by zero, and modulo by zero are parse-time
failures. Constant expressions used as address offsets must evaluate to a value
representable as `I64`.

### 12.5 Range Expressions

Range expressions resolve one scan range in v1.

```txt
range_expr :=
    target_ref
  | module_offset_range
  | module_sized_range
  | address_sized_range

module_offset_range := module_target '[' const_expr '..' const_expr ']'
module_sized_range  := module_target '[' const_expr ':+' const_expr ']'
address_sized_range := address_expr ':+' size
module_target       := target_ref with a module_name
size                := integer
                     | '(' const_expr ')'
```

Rules:

```txt
module target as range means module.base..module.base + module.size
whole-process target as range means all candidate readable regions
module bracket ranges are module-relative offsets
range endpoints are half-open
address_sized_range starts at resolved address_expr
```

Examples:

```txt
<client.dll>
<game.exe!client.dll>
<game.exe!>
<game.exe!client.dll>[0x1000..0x5000]
<game.exe!client.dll>[0x1000:+0x4000]
<client.dll>+0x123:+0x500
```

For v1, whole-process ranges are allowed only for scans. They are invalid as
standalone address expressions and invalid for peek or poke.

### 12.6 Top-Level Grammar

```txt
memory_expr :=
    poke_expr
  | value_scan_expr
  | peek_expr
  | pattern_scan_expr
  | address_expr

peek_expr         := address_expr ws ':' ws type
poke_expr         := address_expr ws ':' ws type ws '=' ws value
value_scan_expr   := range_expr ws ':' ws type ws '==' ws value
pattern_scan_expr := range_expr ws? '{' pattern '}'
```

Parsing chooses the longest valid top-level form. `=` is valid only for poke.
`==` is valid only for value scan. Lexing must recognize `==` before `=`.

Whitespace rules:

```txt
whitespace is allowed around top-level ':', '=', '==', and pattern braces
whitespace is allowed inside parenthesized constant expressions
whitespace is allowed between bytes inside patterns
whitespace is not otherwise allowed inside address_expr or range_expr
```

### 12.7 Parsed Representation

The exact struct layout may evolve during implementation, but `memmy_expr` must
expose the expression kind and enough parsed data for `memmy_exec` to resolve
and execute the expression without reparsing command-line arguments.

Required parser:

```c
typedef struct Memmy_MemoryExpr Memmy_MemoryExpr;

Memmy_Status Memmy_MemoryExpr_Parse(Arena *arena,
                                    String8 text,
                                    Memmy_MemoryExpr *out,
                                    Memmy_Error *error);
```

Recommended parsed expression layering:

```txt
Memmy_TargetExpr
  optional process selector plus module or whole-process target

Memmy_AddressExpr
  target-relative address expression and pointer-chain ops

Memmy_RangeExpr
  target/module/address range expression

Memmy_MemoryExpr
  top-level dispatch expression
```

Process enumeration and process opening remain CLI-owned. `memmy_expr` should
parse process selectors but should not enumerate processes, open processes, read
memory, write memory, or scan memory.

## 13. Expression Execution

`memmy_exec` consumes parsed `Memmy_MemoryExpr` values from `memmy_expr` and
core process/module/region data from `memmy`. It owns process-relative
resolution and execution mechanics, but not CLI process selection or output
formatting.

Responsibilities:

```txt
resolve module targets against Memmy_ModuleList
resolve address expressions, including pointer-chain reads
resolve range expressions, including module and whole-process ranges
lower peek/poke/scan expressions into core memmy read/write/scan calls
return structured results and Memmy_Error values
```

Non-responsibilities:

```txt
parse argv
select a process by --pid or --name
resolve process names or process-qualified targets by enumerating processes
format text, JSON, or JSONL output
perform interactive confirmation or REPL policy
```

`memmy_exec` may require the caller to provide an already-open `Memmy_Process *`
and any needed module or region lists. `memmy_cli` is responsible for opening
the process and collecting those lists based on requirements derived from the
parsed expression.

`memmy_exec` exposes a requirement-derivation API so callers can parse first,
open the process with the right access, and collect only the module or region
data needed by the expression.

```c
typedef struct Memmy_ExecRequirements
{
    Memmy_BackendCap backend_caps;
    Memmy_ProcessAccess process_access;
    B32 needs_external_process;
    B32 needs_modules;
    B32 needs_regions;
} Memmy_ExecRequirements;

Memmy_Status Memmy_MemoryExpr_GetRequirements(Memmy_MemoryExpr *expr,
                                              Memmy_ExecRequirements *out,
                                              Memmy_Error *error);
```

Rules:

```txt
needs_external_process is true when the expression has an absolute address or
unqualified module target and no process selector inside the expression

needs_modules is true when resolving any module target or module-relative range

needs_regions is true for whole-process ranges

process_access includes Read for peek, pointer-chain resolution, and scans

process_access includes Write for poke

memmy_exec never sets ListProcs; parsed process selectors are reported through
the parsed expression so memmy_cli can resolve and open the target process
```

## 14. Scanning

Scans operate on either one caller-provided v0 range or one v1 expression
range. Whole-process expression scans operate over all candidate readable
regions.

```c
typedef struct Memmy_ScanOptions
{
    Memmy_Range range;
    U64 limit;
    U64 chunk_size;
} Memmy_ScanOptions;

typedef struct Memmy_ScanResult
{
    ListLink link;
    Memmy_Addr address;
} Memmy_ScanResult;

typedef struct Memmy_ScanResultList
{
    List list; // Memmy_ScanResult
} Memmy_ScanResultList;
```

```c
Memmy_Status Memmy_Process_ScanValue(Arena *arena,
                                     Memmy_Process *process,
                                     Memmy_ScanOptions *options,
                                     Memmy_Value value,
                                     Memmy_ScanResultList *out,
                                     Memmy_Error *error);

Memmy_Status Memmy_Process_ScanPattern(Arena *arena,
                                       Memmy_Process *process,
                                       Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern,
                                       Memmy_ScanResultList *out,
                                       Memmy_Error *error);
```

Scan rules:

```txt
reads stay inside the specified range or candidate readable region
chunking is allowed
matches may cross chunk boundaries within adjacent readable bytes
limit is a maximum result count
result addresses are absolute
```

`scan` and `pscan` require `Read`. Explicit v0 ranges do not require
`ListRegions`. Expression scan requirements are derived from the parsed range:

```txt
whole-process range   requires ListRegions
module range          requires ListModules
address-sized range   requires ListModules only if its address_expr uses a module target
absolute address-sized range requires no module or region listing
```

Whole-process expression scans require `ListRegions` so that candidate ranges
can be discovered.

If `ListRegions` is available, scans may intersect requested ranges with
committed readable regions to avoid impossible reads. If `ListRegions` is
unavailable and the expression does not require a whole-process range, scans may
attempt chunked reads directly within the requested range and report unreadable
chunks according to scanner error rules.

## 15. CLI

Global form:

```txt
memmy [global-options] <command> [command-options]
memmy [global-options] [target-options] --expr <memory-expr>
```

Global options:

```txt
--json
--jsonl
--help
--version
```

Top-level expression option:

```txt
--expr <memory-expr>
```

Commands:

```txt
procs   List processes.
mods    List modules for a process.
regions List memory regions for a process.
peek    Read one absolute address.
poke    Write one absolute address.
scan    Scan one absolute range for a typed value.
pscan   Scan one absolute range for a byte pattern.
```

Target options:

```txt
--pid <pid>
--name <name>
```

`--name` fails with `Memmy_Status_Ambiguous` if it matches multiple processes.

Command requirements:

```txt
Command  Backend caps     Process access
procs    ListProcs        none
mods     ListModules      Query
regions  ListRegions      Query
peek     Read             Read
poke     Read, Write      Read | Write
scan     Read             Read
pscan    Read             Read
--expr   depends on expression shape
```

Expression process selection may add `ListProcs` when a process name is used in
the expression.

### 15.1 `procs`

```txt
memmy procs
memmy procs --filter chrome
memmy procs --json
```

Text:

```txt
PID     ARCH   NAME
1234    x64    game.exe
```

JSON:

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

### 15.2 `mods`

```txt
memmy mods --pid 1234
memmy mods --pid 1234 --filter kernel32
memmy mods --pid 1234 --json
```

Text:

```txt
BASE                SIZE        NAME
0x00007ff800000000  0x1a4000    game.exe
```

### 15.3 `regions`

```txt
memmy regions --pid 1234
memmy regions --pid 1234 --json
```

Text:

```txt
BASE                END                 SIZE        ACCESS  STATE
0x000001d800000000  0x000001d800010000  0x10000     rw-     committed
```

JSON:

```json
[
  {
    "base": "0x000001d800000000",
    "end": "0x000001d800010000",
    "size": "0x10000",
    "access": "rw-",
    "state": "committed"
  }
]
```

`END` is computed as `base + size`. If backend or test data would overflow,
the command reports `Memmy_Status_Overflow` instead of wrapping.

### 15.4 `peek`

```txt
memmy peek --pid 1234 --addr <addr> --type <type>
memmy peek --pid 1234 --addr <addr> --type bytes --count <count>
memmy peek --pid 1234 --addr <addr> --type str --count <count>
memmy peek --pid 1234 --addr <addr> --type wstr --count <count>
```

`peek` does not accept `--expr`. Use top-level `memmy --expr` for expression
peeks.

Text:

```txt
0x000001d856780004: u32 1337  0x00000539
```

JSON:

```json
{
  "address": "0x000001d856780004",
  "type": "u32",
  "value": 1337,
  "hex": "0x00000539"
}
```

### 15.5 `poke`

```txt
memmy poke --pid 1234 --addr <addr> --type <type> --value <value>
memmy poke --pid 1234 --addr <addr> --type bytes --value "90 90 90"
memmy poke --pid 1234 --addr <addr> --type str --value "hello"
memmy poke --pid 1234 --addr <addr> --type wstr --value "hello"
memmy poke --pid 1234 --addr <addr> --type u32 --value 1337 --dry-run
```

`poke` does not accept `--expr`. Use top-level `memmy --expr` for expression
pokes.

Dry-run text:

```txt
would write:
  process: game.exe 1234
  address: 0x000001d856780004
  type:    u32
  old:     100  / 0x00000064
  new:     1337 / 0x00000539
```

### 15.6 `scan`

```txt
memmy scan --pid 1234 --start <addr> --end <addr> --type <type> --value <value>
memmy scan --pid 1234 --start <addr> --length <size> --type <type> --value <value>
memmy scan --pid 1234 --start <addr> --length <size> --limit <count> --type u32 --value 1337
memmy scan --pid 1234 --start <addr> --length <size> --chunk-size <bytes> --type bytes --value "48 8B"
```

`scan` does not accept `--expr`. Use top-level `memmy --expr` for expression
value scans.

### 15.7 `pscan`

```txt
memmy pscan --pid 1234 --start <addr> --end <addr> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --limit <count> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --chunk-size <bytes> --pattern <pattern>
```

`pscan` does not accept `--expr`. Use top-level `memmy --expr` for expression
pattern scans.

Scan text output:

```txt
ADDRESS
0x00007ff800004242
0x00007ff800007abc
```

Scan JSONL:

```json
{"address":"0x00007ff800004242"}
{"address":"0x00007ff800007abc"}
```

### 15.8 Top-Level `--expr`

Top-level `memmy --expr <memory-expr>` dispatches by expression kind:

```txt
address expression      resolve address and print it
peek expression         read and print one typed value
poke expression         write one typed value
pattern scan expression run pattern scan
value scan expression   run exact value scan
```

`memmy_cli` parses CLI options and resolves the target process. It delegates
expression parsing to `memmy_expr`, then delegates address/range resolution and
expression execution to `memmy_exec`. `memmy_cli` remains responsible for text,
JSON, and JSONL formatting.

Examples:

```txt
memmy --pid 1234 --expr "<client.dll>+0x123"
memmy --pid 1234 --expr "<client.dll>+0x123->0x8"
memmy --expr "<game.exe!client.dll>+0x123 : i32"
memmy --expr "<game.exe!client.dll>+0x123 : i32 = 77"
memmy --expr "<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}"
memmy --expr "<game.exe!> : i32 == 42"
```

Unqualified module expressions require top-level `--pid` or `--name`.
Qualified expressions may select the process inside the expression. Supplying
both external and expression process selectors is invalid unless they resolve to
the same PID.

Top-level expression requirements:

```txt
Expression form                 Backend caps          Process access
absolute address                none                  none
module address                  ListModules           Query
absolute peek                   Read                  Read
module peek                     Read, ListModules     Read | Query
absolute poke                   Read, Write           Read | Write
module poke                     Read, Write, ListModules Read | Write | Query
module range scan               Read, ListModules     Read | Query
absolute address-sized scan     Read                  Read
module address-sized scan       Read, ListModules     Read | Query
whole-process scan              Read, ListRegions     Read | Query
```

Expression process-name selection also requires `ListProcs`.

Expression value scans are exact encoded-byte scans in v1.

Shell quoting is required for most expressions because `<`, `>`, `{`, `}`, `*`,
and spaces have shell-specific meanings.

## 16. Output Formatting

Addresses are fixed-width lowercase hex:

```txt
64-bit target: 0x00007ff800004242
32-bit target: 0x00404242
```

JSON rules:

```txt
addresses are strings
sizes are strings when hex is clearer
byte arrays are lowercase two-digit hex bytes separated by one space
successful JSON is command-shaped, not enveloped
failures use the standard error object
```

Top-level address-expression text output is one address:

```txt
0x00007ff800004242
```

Top-level address-expression JSON output:

```json
{
  "address": "0x00007ff800004242"
}
```

Top-level peek, poke, pattern scan, and value scan expressions reuse the v0
command-shaped text, JSON, and JSONL output formats for `peek`, `poke`,
`pscan`, and `scan`. Implementations may include expression-specific fields in
JSON only if the v0 fields remain stable.

## 17. Windows Backend

Initial Windows backend APIs may include:

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

v1 must support same-bitness targets and 64-bit hosts inspecting 64-bit or
WOW64 32-bit targets. Other cross-bitness cases may return
`Memmy_Status_Unsupported`.

## 18. Testing

Current test layout:

```txt
unittest/test_framework.*       small test framework
unittest/unittest_main.c        test runner entry point
unittest/base/                  base library tests
unittest/memmy/                 memmy, memmy_expr, memmy_exec, and memmy_cli tests
unittest/memmy/test_memmy_expr_*.c expression parser tests
unittest/memmy/test_memmy_exec_*.c expression resolution/execution tests
unittest/memmy/test_memmy_cli_*.c separate CLI command tests
unittest/memmy/test_memmy_backend.* fake backend for core and CLI tests
```

The `memmy_test` target links `memmy_cli`, `memmy_exec`, `memmy_expr`, `memmy`,
and `base` so CLI and expression behavior can be tested without invoking the
`cmd_memmy` executable. v1 should continue to test most behavior at the library
boundary and reserve executable-level tests for smoke coverage.

Important v1 tests:

```txt
all v0 tests
memmy_expr parser tests
memmy_exec address and range resolution tests
memmy_exec expression execution tests
memmy_cli top-level --expr option parsing
target ref parsing
qualified and unqualified process selection
external --pid/--name conflict with process-qualified expressions
ambiguous process-name handling inside expressions
absolute address expressions
module name lookup for address expressions
address expression add and sub
pointer-chain resolution with target pointer width
constant expression parsing for offsets
module-relative range expression parsing
absolute address-sized range expression parsing
module-relative pattern scan expression dispatch
whole-process value scan expression dispatch
peek expression dispatch
poke expression dispatch
lex == before =
top-level --expr validation
subcommands reject --expr
expression parse errors with byte offsets
JSON output for top-level address expressions
```

Required acceptance examples:

```txt
0x000001d856780004 : u32
<client.dll>+0x123
<client.dll>+0x123->0x8
<game.exe!client.dll>+0x123 : i32
<game.exe!client.dll>+0x123 : i32 = 77
<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}
<game.exe!> : i32 == 42
```

The test backend should provide a fixed memory buffer, fake process metadata,
fake modules, fake regions, configurable pointer width, and read/write
operations.

## 19. Milestones

```txt
1. Preserve v0 behavior and tests
2. Add memmy_expr target and public header
3. memmy_expr target refs and constant expressions
4. memmy_expr address expressions
5. memmy_expr top-level memory expressions
6. Add memmy_exec target and public header
7. memmy_exec requirements derivation
8. memmy_exec module and absolute address resolution
9. memmy_exec pointer-chain resolution
10. memmy_cli top-level --expr address resolution
11. memmy_exec peek and poke execution
12. memmy_cli top-level --expr peek and poke
13. memmy_exec module and address-sized range resolution
14. memmy_cli top-level --expr pattern scan
15. memmy_exec whole-process expression ranges
16. memmy_cli top-level --expr value scan
17. Agent output: --json, --jsonl, stable expression errors
18. Final v1 audit
```
