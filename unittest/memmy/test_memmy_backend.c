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
    Memmy_ProcessInfo *info = Arena_PushStruct(arena, Memmy_ProcessInfo);
    info->pid = Test_MemmyBackend_Pid;
    info->name = String8_Lit("test-process");
    info->path = String8_Lit("C:\\test\\test-process.exe");
    info->pointer_width = Memmy_PointerWidth_64;
    List_PushBack(&out->list, &info->link);
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_OpenProcess(Arena *arena, U32 pid, Memmy_ProcessAccess access,
                                                  Memmy_Process **out, Memmy_Error *error)
{
    Unused(access);

    if (pid != Test_MemmyBackend_Pid)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("backend"),
                        String8_Lit("test process was not found"));
        return Memmy_Status_NotFound;
    }

    Test_MemmyBackend *test_backend = ContainerOf(Memmy_Context_Get()->backend, Test_MemmyBackend, backend);
    Memmy_Process *process = Arena_PushStruct(arena, Memmy_Process);
    process->backend = &test_backend->backend;
    process->pid = pid;
    process->pointer_width = test_backend->pointer_width;
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

    if (addr < backend->memory_base || addr - backend->memory_base >= TEST_MEMMY_BACKEND_MEMORY_SIZE)
    {
        Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("backend"),
                        String8_Lit("test address is unreadable"));
        return Memmy_Status_Unreadable;
    }

    U64 offset = addr - backend->memory_base;
    U64 available = TEST_MEMMY_BACKEND_MEMORY_SIZE - offset;
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

    if (addr < backend->memory_base || addr - backend->memory_base >= TEST_MEMMY_BACKEND_MEMORY_SIZE)
    {
        Memmy_Error_Set(error, Memmy_Status_Unwritable, String8_Lit("backend"),
                        String8_Lit("test address is unwritable"));
        return Memmy_Status_Unwritable;
    }

    U64 offset = addr - backend->memory_base;
    U64 available = TEST_MEMMY_BACKEND_MEMORY_SIZE - offset;
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
    Unused(process);
    Unused(error);

    *out = (Memmy_ModuleList){0};
    Memmy_Module *module = Arena_PushStruct(arena, Memmy_Module);
    module->name = String8_Lit("test-module.exe");
    module->path = String8_Lit("C:\\test\\test-module.exe");
    module->base = 0x10000000;
    module->size = 0x2000;
    List_PushBack(&out->list, &module->link);
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_ListRegions(Arena *arena, Memmy_Process *process, Memmy_RegionList *out,
                                                  Memmy_Error *error)
{
    Unused(error);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *out = (Memmy_RegionList){0};
    Memmy_Region *region = Arena_PushStruct(arena, Memmy_Region);
    region->base = backend->memory_base;
    region->size = TEST_MEMMY_BACKEND_MEMORY_SIZE;
    region->access = Memmy_RegionAccess_Read | Memmy_RegionAccess_Write;
    region->state = Memmy_RegionState_Committed;
    List_PushBack(&out->list, &region->link);
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
        .pointer_width = Memmy_PointerWidth_64,
        .memory_base = 0x1000,
    };

    for (U64 i = 0; i < TEST_MEMMY_BACKEND_MEMORY_SIZE; i++)
    {
        backend->memory[i] = (U8)i;
    }
}

Memmy_Backend *Test_MemmyBackend_AsBackend(Test_MemmyBackend *backend)
{
    return &backend->backend;
}
