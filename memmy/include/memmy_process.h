#ifndef MEMMY_PROCESS_H
#define MEMMY_PROCESS_H

#include "base_arena.h"
#include "base_list.h"
#include "base_string.h"
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
    ListLink link;
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
};

struct Memmy_ProcessList
{
    List list; // Memmy_ProcessInfo
};

typedef struct Memmy_Module Memmy_Module;
struct Memmy_Module
{
    ListLink link;
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
};

struct Memmy_ModuleList
{
    List list; // Memmy_Module
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
    ListLink link;
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
};

struct Memmy_RegionList
{
    List list; // Memmy_Region
};

Memmy_Status Memmy_ListProcesses(Arena *arena, Memmy_ProcessList *out, Memmy_Error *error);
Memmy_Status Memmy_Process_Open(Arena *arena, U32 pid, Memmy_ProcessAccess access, Memmy_Process **out,
                                Memmy_Error *error);
B32 Memmy_Process_IsOpen(Memmy_Process *process);
void Memmy_Process_Close(Memmy_Process *process);
Memmy_Status Memmy_Process_ListModules(Arena *arena, Memmy_Process *process, Memmy_ModuleList *out, Memmy_Error *error);
Memmy_Status Memmy_Process_ListRegions(Arena *arena, Memmy_Process *process, Memmy_RegionList *out, Memmy_Error *error);
Memmy_Status Memmy_Process_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                                Memmy_Error *error);
Memmy_Status Memmy_Process_Write(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_written,
                                 Memmy_Error *error);

#endif // MEMMY_PROCESS_H
