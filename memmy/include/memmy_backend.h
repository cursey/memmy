#ifndef MEMMY_BACKEND_H
#define MEMMY_BACKEND_H

#include "base.h"
#include "memmy_status.h"
#include "memmy_types.h"

typedef struct Memmy_Module Memmy_Module;
typedef struct Memmy_ModuleSink Memmy_ModuleSink;
typedef struct Memmy_ObjectBaseOptions Memmy_ObjectBaseOptions;
typedef struct Memmy_ObjectBaseResult Memmy_ObjectBaseResult;
typedef struct Memmy_Process Memmy_Process;
typedef struct Memmy_ProcessInfo Memmy_ProcessInfo;
typedef struct Memmy_ProcessInfoSink Memmy_ProcessInfoSink;
typedef struct Memmy_Range Memmy_Range;
typedef struct Memmy_Region Memmy_Region;
typedef struct Memmy_RegionSink Memmy_RegionSink;

typedef Memmy_Status Memmy_ProcessInfoSinkFn(void *user_data, Memmy_ProcessInfo const *info);
struct Memmy_ProcessInfoSink
{
    Memmy_ProcessInfoSinkFn *callback;
    void *user_data;
};

typedef Memmy_Status Memmy_ModuleSinkFn(void *user_data, Memmy_Module const *module);
struct Memmy_ModuleSink
{
    Memmy_ModuleSinkFn *callback;
    void *user_data;
};

typedef Memmy_Status Memmy_RegionSinkFn(void *user_data, Memmy_Region const *region);
struct Memmy_RegionSink
{
    Memmy_RegionSinkFn *callback;
    void *user_data;
};

typedef struct Memmy_Backend Memmy_Backend;
struct Memmy_Backend
{
    String8 name;

    Memmy_Status (*enumerate_processes)(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error);
    Memmy_Status (*open_process)(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error);
    void (*close_process)(Memmy_Process *process);
    Memmy_Status (*read)(Memmy_Process *process, Memmy_Addr address, void *buffer, U64 size, U64 *bytes_read,
                         Memmy_Error *error);
    Memmy_Status (*write)(Memmy_Process *process, Memmy_Addr address, void const *buffer, U64 size, U64 *bytes_written,
                          Memmy_Error *error);
    Memmy_Status (*enumerate_modules)(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink, Memmy_Error *error);
    Memmy_Status (*enumerate_regions)(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink, Memmy_Error *error);
    Memmy_Status (*find_function)(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Range *out,
                                  Memmy_Error *error);
    Memmy_Status (*find_object_base)(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                     Memmy_ObjectBaseOptions const *options, Memmy_ObjectBaseResult *out,
                                     Memmy_Error *error);
};

#endif // MEMMY_BACKEND_H
