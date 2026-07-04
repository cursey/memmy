# Memmy v0 Specification

## 1. Scope

Memmy v0 is a small native CLI and C library for inspecting and modifying
memory in local processes.

v0 deliberately excludes expression evaluation. Memory commands accept absolute
remote addresses only. Scan commands accept exactly one absolute range per
invocation, expressed as either start/end or start/length.

Primary CLI:

```txt
memmy procs
memmy mods  --pid 1234
memmy regions --pid 1234
memmy peek  --pid 1234 --addr 0x000001d856780004 --type u32
memmy poke  --pid 1234 --addr 0x000001d856780004 --type u32 --value 1337
memmy scan  --pid 1234 --start 0x00007ff800000000 --end 0x00007ff8001a4000 --type u32 --value 1337
memmy pscan --pid 1234 --start 0x00007ff800000000 --length 0x1a4000 --pattern "48 8B ?? ?? 89"
```

Windows is the first backend. Linux and macOS are future targets.

## 2. Explicit Non-Goals

v0 does not include:

```txt
address expression parsing or resolution
range expression parsing or resolution
constant-expression parsing or evaluation
module-relative input syntax
pointer-chain resolution
implicit whole-process scans
multiple input addresses per invocation
multiple input ranges per invocation
symbol loading
PDB/DWARF support
repeated narrowing scans
remote machine support
kernel memory support
GUI
scripting
disassembly
remote allocation/free
memory protection changes
thread enumeration or suspension
```

Module lists still exist, but module names are output information only in v0.
They are not accepted as address or range input.

Region lists also exist as discovery output. They help users and agents choose
absolute `--start`, `--end`, and `--length` values without requiring range
syntax.

## 3. Project Shape

```txt
base/
  Shared foundation library.

memmy/
  Core library: process model, address/range parsing, typed values, patterns,
  scanning, formatting, errors, and backend dispatch.

cmd/memmy/
  CLI executable.

vendor/
  Third-party code.
```

v0 should not add `memmy_address_expr.*`, `cmd_addr.c`, or a range-expression
parser. If a parser is needed, it is a numeric address/size parser only.

## 4. Naming and Style

Follow the repository rules in `AGENTS.md`.

Important v0 names:

```c
typedef U64 Memmy_Addr;
typedef U64 Memmy_Size;
```

Remote process addresses are integers, not local pointers, outside platform
backend code.

Public project APIs use `Memmy_` names. Functions use `PascalCase`. Variables
and fields use `snake_case`. Allocation goes through `Arena`.

## 5. Address and Size Input

An address or size input is one unsigned integer token.

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

## 6. Ranges

A range is one half-open interval:

```txt
[start, end)
```

CLI range input uses exactly one of:

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

## 7. Error Model

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

## 8. Backend Boundary

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

    Memmy_Status (*list_processes)(Arena *arena,
                                   Memmy_ProcessList *out,
                                   Memmy_Error *error);

    Memmy_Status (*open_process)(Arena *arena,
                                 U32 pid,
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

Backends report unsupported operations by leaving optional callbacks null or by
returning `Memmy_Status_Unsupported` from the callback.

## 9. Process, Module, and Region Types

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

## 10. Types, Values, and Patterns

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

`pscan --pattern` passes `Memmy_PatternParseFlag_AllowWildcards`. Parsing a
`bytes` value for `peek`, `poke`, or `scan` rejects wildcards.

## 11. Scanning

Scans operate on exactly one caller-provided range.

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
reads stay inside the specified range
chunking is allowed
matches may cross chunk boundaries
limit is a maximum result count
result addresses are absolute
```

`scan` and `pscan` require a process. If region listing is implemented, scans
may intersect the requested range with committed readable regions to avoid
impossible reads. If region listing is unsupported, scans
attempt chunked reads directly within the requested range and report unreadable
chunks according to the scanner error rules.

Region enumeration must not create implicit input ranges.

## 12. CLI

Global form:

```txt
memmy [global-options] <command> [command-options]
```

Global options:

```txt
--json
--jsonl
--help
--version
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

Command behavior:

```txt
Command  Backend operations attempted
procs    list_processes
mods     open_process, list_modules
regions  open_process, list_regions
peek     open_process, read
poke     open_process, read, write
scan     open_process, read, optional list_regions
pscan    open_process, read, optional list_regions
```

### 12.1 `procs`

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

### 12.2 `mods`

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

### 12.3 `regions`

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

### 12.4 `peek`

```txt
memmy peek --pid 1234 --addr <addr> --type <type>
memmy peek --pid 1234 --addr <addr> --type bytes --count <count>
memmy peek --pid 1234 --addr <addr> --type str --count <count>
memmy peek --pid 1234 --addr <addr> --type wstr --count <count>
```

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

### 12.5 `poke`

```txt
memmy poke --pid 1234 --addr <addr> --type <type> --value <value>
memmy poke --pid 1234 --addr <addr> --type bytes --value "90 90 90"
memmy poke --pid 1234 --addr <addr> --type str --value "hello"
memmy poke --pid 1234 --addr <addr> --type wstr --value "hello"
memmy poke --pid 1234 --addr <addr> --type u32 --value 1337 --dry-run
```

Dry-run text:

```txt
would write:
  process: game.exe 1234
  address: 0x000001d856780004
  type:    u32
  old:     100  / 0x00000064
  new:     1337 / 0x00000539
```

### 12.6 `scan`

```txt
memmy scan --pid 1234 --start <addr> --end <addr> --type <type> --value <value>
memmy scan --pid 1234 --start <addr> --length <size> --type <type> --value <value>
memmy scan --pid 1234 --start <addr> --length <size> --limit <count> --type u32 --value 1337
memmy scan --pid 1234 --start <addr> --length <size> --chunk-size <bytes> --type bytes --value "48 8B"
```

### 12.7 `pscan`

```txt
memmy pscan --pid 1234 --start <addr> --end <addr> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --limit <count> --pattern <pattern>
memmy pscan --pid 1234 --start <addr> --length <size> --chunk-size <bytes> --pattern <pattern>
```

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

## 13. Output Formatting

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

## 14. Windows Backend

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

v0 must support same-bitness targets and 64-bit hosts inspecting 64-bit or
WOW64 32-bit targets. Other cross-bitness cases may return
`Memmy_Status_Unsupported`.

## 15. Testing

Important v0 tests:

```txt
address parsing
size parsing
range start/end validation
range start/length overflow validation
region listing
typed value parsing
pattern parsing
bytes value wildcard rejection
peek at absolute address
poke at absolute address
scan within one explicit range
scan with and without ListRegions
chunk-boundary matches
JSON escaping
ambiguous process-name handling
```

The test backend should provide a fixed memory buffer, fake process metadata,
fake modules, fake regions, configurable pointer width, and read/write
operations.

## 16. Milestones

```txt
1. CLI skeleton: memmy --help, procs, mods, regions
2. Numeric helpers: address, size, and range construction
3. Type, value, and pattern parsing
4. Memory read: peek --addr
5. Memory write: poke --addr, --dry-run
6. Pattern scan: pscan --start/--end or --start/--length
7. Value scan: scan --start/--end or --start/--length
8. Agent output: --json, --jsonl, stable errors
```
