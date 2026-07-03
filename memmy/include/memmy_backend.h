#ifndef MEMMY_BACKEND_H
#define MEMMY_BACKEND_H

#include "base_arena.h"
#include "base_string.h"
#include "memmy_status.h"
#include "memmy_types.h"

typedef struct Memmy_ModuleList Memmy_ModuleList;
typedef struct Memmy_Process Memmy_Process;
typedef struct Memmy_ProcessList Memmy_ProcessList;
typedef struct Memmy_RegionList Memmy_RegionList;

typedef U32 Memmy_BackendCap;
enum
{
    Memmy_BackendCap_Read = 1u << 0,
    Memmy_BackendCap_Write = 1u << 1,
    Memmy_BackendCap_ListProcs = 1u << 2,
    Memmy_BackendCap_ListModules = 1u << 3,
    Memmy_BackendCap_ListRegions = 1u << 4,
};

typedef U32 Memmy_ProcessAccess;
enum
{
    Memmy_ProcessAccess_Read = 1u << 0,
    Memmy_ProcessAccess_Write = 1u << 1,
    Memmy_ProcessAccess_Query = 1u << 2,
};

typedef struct Memmy_Backend Memmy_Backend;
struct Memmy_Backend
{
    String8 name;
    U32 capabilities;

    Memmy_Status (*list_processes)(Arena *arena, Memmy_ProcessList *out, Memmy_Error *error);
    Memmy_Status (*open_process)(Arena *arena, U32 pid, Memmy_ProcessAccess access, Memmy_Process **out,
                                 Memmy_Error *error);
    void (*close_process)(Memmy_Process *process);
    Memmy_Status (*read)(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                         Memmy_Error *error);
    Memmy_Status (*write)(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_written,
                          Memmy_Error *error);
    Memmy_Status (*list_modules)(Arena *arena, Memmy_Process *process, Memmy_ModuleList *out, Memmy_Error *error);
    Memmy_Status (*list_regions)(Arena *arena, Memmy_Process *process, Memmy_RegionList *out, Memmy_Error *error);
};

B32 Memmy_Backend_HasCapability(Memmy_Backend *backend, Memmy_BackendCap capability);

#endif // MEMMY_BACKEND_H
