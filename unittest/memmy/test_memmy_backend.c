#include "test_memmy_backend.h"

#include <string.h>

enum
{
    Test_MemmyBackend_Pid = 4242,
};

static Memmy_Status Test_MemmyBackend_ListProcesses(Arena *arena, Memmy_ProcessList *out, Memmy_Error *error)
{
    Unused(error);

    *out = (Memmy_ProcessList){0};
    Test_MemmyBackend *backend = ContainerOf(Memmy_Context_Get()->backend, Test_MemmyBackend, backend);
    for (U64 i = 0; i < backend->process_count; i++)
    {
        Test_MemmyBackendProcess *src = &backend->processes[i];
        Memmy_ProcessInfo *info = Memmy_ProcessList_Push(arena, out);
        info->pid = src->pid;
        info->name = src->name;
        info->path = src->path;
        info->pointer_width = src->pointer_width;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_OpenProcess(Arena *arena, U32 pid, Memmy_ProcessAccess access,
                                                  Memmy_Process **out, Memmy_Error *error)
{
    Unused(access);

    Test_MemmyBackend *test_backend = ContainerOf(Memmy_Context_Get()->backend, Test_MemmyBackend, backend);
    Test_MemmyBackendProcess *info = 0;
    for (U64 i = 0; i < test_backend->process_count; i++)
    {
        if (test_backend->processes[i].pid == pid)
        {
            info = &test_backend->processes[i];
            break;
        }
    }

    if (info == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("backend"),
                        String8_Lit("test process was not found"));
        return Memmy_Status_NotFound;
    }

    Memmy_Process *process = Arena_PushStruct(arena, Memmy_Process);
    process->backend = &test_backend->backend;
    process->pid = pid;
    process->pointer_width = info->pointer_width;
    process->backend_data = test_backend;
    *out = process;
    return Memmy_Status_Ok;
}

static void Test_MemmyBackend_CloseProcess(Memmy_Process *process)
{
    process->backend_data = 0;
}

static Memmy_Status Test_MemmyBackend_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
                                           U64 *bytes_read, Memmy_Error *error)
{
    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *bytes_read = 0;

    if (backend->read_status != Memmy_Status_Ok)
    {
        Memmy_Error_Set(error, backend->read_status, String8_Lit("backend"), String8_Lit("test read failure"));
        return backend->read_status;
    }

    if (addr < backend->memory_base || addr - backend->memory_base >= TEST_MEMMY_BACKEND_MEMORY_SIZE)
    {
        Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("backend"),
                        String8_Lit("test address is unreadable"));
        return Memmy_Status_Unreadable;
    }

    U64 offset = addr - backend->memory_base;
    U64 available = TEST_MEMMY_BACKEND_MEMORY_SIZE - offset;
    if (backend->read_limit != 0)
    {
        available = Min(available, backend->read_limit);
    }
    U64 to_read = Min(size, available);
    memcpy(buffer, backend->memory + offset, (size_t)to_read);
    *bytes_read = to_read;

    if (to_read != size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("backend"),
                        String8_Lit("test read crossed memory end"));
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_Write(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
                                            U64 *bytes_written, Memmy_Error *error)
{
    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *bytes_written = 0;

    if (backend->write_status != Memmy_Status_Ok)
    {
        Memmy_Error_Set(error, backend->write_status, String8_Lit("backend"), String8_Lit("test write failure"));
        return backend->write_status;
    }

    if (addr < backend->memory_base || addr - backend->memory_base >= TEST_MEMMY_BACKEND_MEMORY_SIZE)
    {
        Memmy_Error_Set(error, Memmy_Status_Unwritable, String8_Lit("backend"),
                        String8_Lit("test address is unwritable"));
        return Memmy_Status_Unwritable;
    }

    U64 offset = addr - backend->memory_base;
    U64 available = TEST_MEMMY_BACKEND_MEMORY_SIZE - offset;
    if (backend->write_limit != 0)
    {
        available = Min(available, backend->write_limit);
    }
    U64 to_write = Min(size, available);
    memcpy(backend->memory + offset, buffer, (size_t)to_write);
    *bytes_written = to_write;

    if (to_write != size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialWrite, String8_Lit("backend"),
                        String8_Lit("test write crossed memory end"));
        return Memmy_Status_PartialWrite;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_ListModules(Arena *arena, Memmy_Process *process, Memmy_ModuleList *out,
                                                  Memmy_Error *error)
{
    Unused(error);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *out = (Memmy_ModuleList){0};
    for (U64 i = 0; i < backend->module_count; i++)
    {
        Test_MemmyBackendModule *src = &backend->modules[i];
        if (src->pid == process->pid)
        {
            Memmy_Module *module = Memmy_ModuleList_Push(arena, out);
            module->name = src->name;
            module->path = src->path;
            module->base = src->base;
            module->size = src->size;
        }
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_ListRegions(Arena *arena, Memmy_Process *process, Memmy_RegionList *out,
                                                  Memmy_Error *error)
{
    Unused(error);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *out = (Memmy_RegionList){0};
    for (U64 i = 0; i < backend->region_count; i++)
    {
        Test_MemmyBackendRegion *src = &backend->regions[i];
        if (src->pid == process->pid)
        {
            Memmy_Region *region = Memmy_RegionList_Push(arena, out);
            region->base = src->base;
            region->size = src->size;
            region->access = src->access;
            region->state = src->state;
        }
    }
    return Memmy_Status_Ok;
}

void Test_MemmyBackend_Init(Test_MemmyBackend *backend)
{
    *backend = (Test_MemmyBackend){
        .backend =
            {
                .name = String8_Lit("test"),
                .capabilities = Memmy_BackendCap_Read | Memmy_BackendCap_Write | Memmy_BackendCap_ListProcs |
                                Memmy_BackendCap_ListModules | Memmy_BackendCap_ListRegions,
                .list_processes = Test_MemmyBackend_ListProcesses,
                .open_process = Test_MemmyBackend_OpenProcess,
                .close_process = Test_MemmyBackend_CloseProcess,
                .read = Test_MemmyBackend_Read,
                .write = Test_MemmyBackend_Write,
                .list_modules = Test_MemmyBackend_ListModules,
                .list_regions = Test_MemmyBackend_ListRegions,
            },
        .memory_base = 0x1000,
        .read_status = Memmy_Status_Ok,
        .read_limit = 0,
        .write_status = Memmy_Status_Ok,
        .write_limit = 0,
    };

    Test_MemmyBackend_AddProcess(backend, Test_MemmyBackend_Pid, String8_Lit("test-process"),
                                 String8_Lit("C:\\test\\test-process.exe"), Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(backend, Test_MemmyBackend_Pid, String8_Lit("test-module.exe"),
                                String8_Lit("C:\\test\\test-module.exe"), 0x10000000, 0x2000);
    Test_MemmyBackend_AddRegion(backend, Test_MemmyBackend_Pid, backend->memory_base, TEST_MEMMY_BACKEND_MEMORY_SIZE,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    for (U64 i = 0; i < TEST_MEMMY_BACKEND_MEMORY_SIZE; i++)
    {
        backend->memory[i] = (U8)i;
    }
}

Memmy_Backend *Test_MemmyBackend_AsBackend(Test_MemmyBackend *backend)
{
    return &backend->backend;
}

Test_MemmyBackendProcess *Test_MemmyBackend_AddProcess(Test_MemmyBackend *backend, U32 pid, String8 name, String8 path,
                                                       Memmy_PointerWidth pointer_width)
{
    if (backend->process_count >= TEST_MEMMY_BACKEND_MAX_PROCESSES)
    {
        return 0;
    }

    Test_MemmyBackendProcess *process = &backend->processes[backend->process_count++];
    *process = (Test_MemmyBackendProcess){
        .pid = pid,
        .name = name,
        .path = path,
        .pointer_width = pointer_width,
    };
    return process;
}

Test_MemmyBackendModule *Test_MemmyBackend_AddModule(Test_MemmyBackend *backend, U32 pid, String8 name, String8 path,
                                                     Memmy_Addr base, Memmy_Size size)
{
    if (backend->module_count >= TEST_MEMMY_BACKEND_MAX_MODULES)
    {
        return 0;
    }

    Test_MemmyBackendModule *module = &backend->modules[backend->module_count++];
    *module = (Test_MemmyBackendModule){
        .pid = pid,
        .name = name,
        .path = path,
        .base = base,
        .size = size,
    };
    return module;
}

Test_MemmyBackendRegion *Test_MemmyBackend_AddRegion(Test_MemmyBackend *backend, U32 pid, Memmy_Addr base,
                                                     Memmy_Size size, Memmy_RegionAccess access,
                                                     Memmy_RegionState state)
{
    if (backend->region_count >= TEST_MEMMY_BACKEND_MAX_REGIONS)
    {
        return 0;
    }

    Test_MemmyBackendRegion *region = &backend->regions[backend->region_count++];
    *region = (Test_MemmyBackendRegion){
        .pid = pid,
        .base = base,
        .size = size,
        .access = access,
        .state = state,
    };
    return region;
}

void Test_MemmyBackend_SetMemoryBase(Test_MemmyBackend *backend, Memmy_Addr base)
{
    backend->memory_base = base;
}

void Test_MemmyBackend_SetReadStatus(Test_MemmyBackend *backend, Memmy_Status status)
{
    backend->read_status = status;
}

void Test_MemmyBackend_SetReadLimit(Test_MemmyBackend *backend, U64 limit)
{
    backend->read_limit = limit;
}

void Test_MemmyBackend_SetWriteStatus(Test_MemmyBackend *backend, Memmy_Status status)
{
    backend->write_status = status;
}

void Test_MemmyBackend_SetWriteLimit(Test_MemmyBackend *backend, U64 limit)
{
    backend->write_limit = limit;
}
