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

typedef struct Memmy_Backend Memmy_Backend;
struct Memmy_Backend
{
    String8 name;

    Memmy_Status (*list_processes)(Arena *arena, Memmy_ProcessList *out, Memmy_Error *error);
    Memmy_Status (*open_process)(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error);
    void (*close_process)(Memmy_Process *process);
    Memmy_Status (*read)(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                         Memmy_Error *error);
    Memmy_Status (*write)(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_written,
                          Memmy_Error *error);
    Memmy_Status (*list_modules)(Arena *arena, Memmy_Process *process, Memmy_ModuleList *out, Memmy_Error *error);
    Memmy_Status (*list_regions)(Arena *arena, Memmy_Process *process, Memmy_RegionList *out, Memmy_Error *error);
};

#endif // MEMMY_BACKEND_H
