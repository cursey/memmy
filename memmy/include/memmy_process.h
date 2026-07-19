#ifndef MEMMY_PROCESS_H
#define MEMMY_PROCESS_H

#include "base.h"
#include "memmy_backend.h"
#include "memmy_status.h"
#include "memmy_types.h"

typedef U32 Memmy_PointerWidth;
enum
{
    Memmy_PointerWidth_Unknown,
    Memmy_PointerWidth_32,
    Memmy_PointerWidth_64,
};

struct Memmy_Process
{
    Memmy_Backend *backend;
    U32 pid;
    Memmy_PointerWidth pointer_width;
    void *backend_data;
};

typedef struct Memmy_ProcessInfo Memmy_ProcessInfo;
struct Memmy_ProcessInfo
{
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
};

typedef struct Memmy_Module Memmy_Module;
struct Memmy_Module
{
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
};

typedef U32 Memmy_RegionAccess;
enum
{
    Memmy_RegionAccess_Read = 1u << 0,
    Memmy_RegionAccess_Write = 1u << 1,
    Memmy_RegionAccess_Execute = 1u << 2,
    Memmy_RegionAccess_Guard = 1u << 3,
};

typedef U32 Memmy_RegionState;
enum
{
    Memmy_RegionState_Free,
    Memmy_RegionState_Reserved,
    Memmy_RegionState_Committed,
};

typedef struct Memmy_Region Memmy_Region;
struct Memmy_Region
{
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
};

typedef Memmy_Status Memmy_RangeSinkFn(void *user_data, Memmy_Range range);
typedef struct Memmy_RangeSink Memmy_RangeSink;
struct Memmy_RangeSink
{
    Memmy_RangeSinkFn *callback;
    void *user_data;
};

typedef U32 Memmy_ObjectBaseConfidence;
enum
{
    Memmy_ObjectBaseConfidence_None,
    Memmy_ObjectBaseConfidence_Weak,
    Memmy_ObjectBaseConfidence_Strong,
};

typedef struct Memmy_ObjectBaseOptions Memmy_ObjectBaseOptions;
struct Memmy_ObjectBaseOptions
{
    U64 max_scan_back;
    U32 min_vtable_entries;
};

typedef struct Memmy_ObjectBaseResult Memmy_ObjectBaseResult;
struct Memmy_ObjectBaseResult
{
    Memmy_Addr address;
    Memmy_Addr vptr_address;
    Memmy_Addr vtable;
    Memmy_ObjectBaseConfidence confidence;
    String8 type_name;
};

// Enumeration strings belong to arena; metadata pointers are valid only during callbacks. Error is optional.
Memmy_Status Memmy_Process_Enumerate(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error);
// On return, including failure, out is initialized to null. The process belongs to arena.
Memmy_Status Memmy_Process_Open(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error);
B32 Memmy_Process_IsOpen(Memmy_Process *process);
void Memmy_Process_Close(Memmy_Process *process);
Memmy_Status Memmy_Process_EnumerateModules(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink,
                                            Memmy_Error *error);
Memmy_Status Memmy_Process_EnumerateRegions(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink,
                                            Memmy_Error *error);
// Address ranges are concrete, half-open, and start at zero.
Memmy_Status Memmy_Process_GetAddressRange(Memmy_Process *process, Memmy_Range *out, Memmy_Error *error);
// Qualifying regions are clipped, sorted, and merged into ordered maximal ranges.
Memmy_Status Memmy_Process_EnumerateAccessibleRanges(Arena *arena, Memmy_Process *process, Memmy_Range bounds,
                                                     Memmy_RegionAccess required_access, Memmy_RangeSink sink,
                                                     Memmy_Error *error);
// Find outputs are initialized before validation. Any returned strings belong to arena.
Memmy_Status Memmy_Process_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Range *out,
                                        Memmy_Error *error);
Memmy_Status Memmy_Process_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                          Memmy_ObjectBaseOptions const *options, Memmy_ObjectBaseResult *out,
                                          Memmy_Error *error);
// Required outputs are cleared before validation. Enumeration metadata is valid only during its callback.
Memmy_Status Memmy_Process_Read(Memmy_Process *process, Memmy_Addr address, void *buffer, U64 size, U64 *bytes_read,
                                Memmy_Error *error);
Memmy_Status Memmy_Process_Write(Memmy_Process *process, Memmy_Addr address, void const *buffer, U64 size,
                                 U64 *bytes_written, Memmy_Error *error);

#endif // MEMMY_PROCESS_H
