#include "test_memmy_backend.h"

#include <string.h>

enum
{
    Test_MemmyBackend_Pid = 4242,
};

static Memmy_Status Test_MemmyBackend_EnumerateProcesses(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error)
{
    Unused(error);

    Test_MemmyBackend *backend = ContainerOf(Memmy_Context_Get()->backend, Test_MemmyBackend, backend);
    for (U64 i = 0; i < backend->process_count; i++)
    {
        Test_MemmyBackendProcess *src = &backend->processes[i];
        Memmy_ProcessInfo info = {
            .pid = src->pid,
            .name = src->name,
            .path = src->path,
            .pointer_width = src->pointer_width,
        };
        if (backend->process_info_strings_use_enum_arena)
        {
            info.name = String8_Copy(arena, info.name);
            info.path = String8_Copy(arena, info.path);
        }
        Memmy_Status status = sink.callback(sink.user_data, &info);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_OpenProcess(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
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

    test_backend->open_call_count++;
    test_backend->last_open_pid = pid;

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
    Test_MemmyBackend *test_backend = (Test_MemmyBackend *)process->backend_data;
    test_backend->close_call_count++;
    test_backend->last_close_pid = process->pid;
    process->backend_data = 0;
}

static Memmy_Status Test_MemmyBackend_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
                                           U64 *bytes_read, Memmy_Error *error)
{
    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    *bytes_read = 0;
    backend->read_call_count++;
    backend->min_read_addr = backend->min_read_addr == 0 ? addr : Min(backend->min_read_addr, addr);
    backend->max_read_end = Max(backend->max_read_end, addr + size);

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
    Memmy_Addr readable_end = backend->memory_base + TEST_MEMMY_BACKEND_MEMORY_SIZE;
    Memmy_Addr request_end = addr + Min(size, readable_end - addr);
    for (U64 i = 0; i < backend->unreadable_range_count; i++)
    {
        Test_MemmyBackendUnreadableRange *range = &backend->unreadable_ranges[i];
        if (addr >= range->start && addr < range->end)
        {
            Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("backend"),
                            String8_Lit("test address is in an unreadable range"));
            return Memmy_Status_Unreadable;
        }
        if (range->start > addr && range->start < request_end)
        {
            available = Min(available, range->start - addr);
        }
    }
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

static Memmy_Status Test_MemmyBackend_Write(Memmy_Process *process, Memmy_Addr addr, void const *buffer, U64 size,
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

static Memmy_Status Test_MemmyBackend_EnumerateModules(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink,
                                                       Memmy_Error *error)
{
    Unused(arena);
    Unused(error);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    for (U64 i = 0; i < backend->module_count; i++)
    {
        Test_MemmyBackendModule *src = &backend->modules[i];
        if (src->pid == process->pid)
        {
            Memmy_Module module = {
                .name = src->name,
                .path = src->path,
                .base = src->base,
                .size = src->size,
            };
            Memmy_Status status = sink.callback(sink.user_data, &module);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_EnumerateRegions(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink,
                                                       Memmy_Error *error)
{
    Unused(arena);
    Unused(error);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    for (U64 i = 0; i < backend->region_count; i++)
    {
        Test_MemmyBackendRegion *src = &backend->regions[i];
        if (src->pid == process->pid)
        {
            Memmy_Region region = {
                .base = src->base,
                .size = src->size,
                .access = src->access,
                .state = src->state,
            };
            Memmy_Status status = sink.callback(sink.user_data, &region);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Test_MemmyBackend_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                                   Memmy_Range *out, Memmy_Error *error)
{
    Unused(arena);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    for (U64 i = 0; i < backend->function_count; i++)
    {
        Test_MemmyBackendFunction *function = &backend->functions[i];
        if (function->pid == process->pid && address >= function->range.start && address < function->range.end)
        {
            *out = function->range;
            return Memmy_Status_Ok;
        }
    }

    Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("function"), String8_Lit("function metadata not found"));
    return Memmy_Status_NotFound;
}

static B32 Test_MemmyBackend_RegionContains(Test_MemmyBackendRegion *region, Memmy_Addr address)
{
    Memmy_Addr end = region->base + region->size;
    return address >= region->base && address < end && end >= region->base;
}

static Test_MemmyBackendRegion *Test_MemmyBackend_FindRegion(Test_MemmyBackend *backend, U32 pid, Memmy_Addr address)
{
    for (U64 i = 0; i < backend->region_count; i++)
    {
        Test_MemmyBackendRegion *region = &backend->regions[i];
        if (region->pid == pid && Test_MemmyBackend_RegionContains(region, address))
        {
            return region;
        }
    }
    return 0;
}

static B32 Test_MemmyBackend_IsReadableCommitted(Test_MemmyBackendRegion *region)
{
    return region != 0 && region->state == Memmy_RegionState_Committed &&
           (region->access & Memmy_RegionAccess_Read) != 0 && (region->access & Memmy_RegionAccess_Guard) == 0;
}

static B32 Test_MemmyBackend_IsExecutableCommitted(Test_MemmyBackendRegion *region)
{
    return region != 0 && region->state == Memmy_RegionState_Committed &&
           (region->access & Memmy_RegionAccess_Execute) != 0 && (region->access & Memmy_RegionAccess_Guard) == 0;
}

static B32 Test_MemmyBackend_ReadPointer(Test_MemmyBackend *backend, Memmy_Process *process, Memmy_Addr address,
                                         Memmy_Addr *out)
{
    U64 offset = 0;
    U64 pointer_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    if (address < backend->memory_base || address - backend->memory_base > TEST_MEMMY_BACKEND_MEMORY_SIZE ||
        pointer_size > TEST_MEMMY_BACKEND_MEMORY_SIZE - (address - backend->memory_base))
    {
        return 0;
    }

    offset = address - backend->memory_base;
    if (pointer_size == 4)
    {
        U32 value = 0;
        memcpy(&value, backend->memory + offset, sizeof(value));
        *out = value;
    }
    else
    {
        U64 value = 0;
        memcpy(&value, backend->memory + offset, sizeof(value));
        *out = value;
    }
    return 1;
}

static B32 Test_MemmyBackend_IsPlausibleVtable(Test_MemmyBackend *backend, Memmy_Process *process, Memmy_Addr vtable,
                                               U32 min_vtable_entries)
{
    Test_MemmyBackendRegion *vtable_region = Test_MemmyBackend_FindRegion(backend, process->pid, vtable);
    if (!Test_MemmyBackend_IsReadableCommitted(vtable_region))
    {
        return 0;
    }

    U64 pointer_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    for (U32 i = 0; i < min_vtable_entries; i++)
    {
        Memmy_Addr entry_addr = vtable + (U64)i * pointer_size;
        if (!Test_MemmyBackend_RegionContains(vtable_region, entry_addr))
        {
            return 0;
        }

        Memmy_Addr function_addr = 0;
        if (!Test_MemmyBackend_ReadPointer(backend, process, entry_addr, &function_addr))
        {
            return 0;
        }

        Test_MemmyBackendRegion *function_region = Test_MemmyBackend_FindRegion(backend, process->pid, function_addr);
        if (!Test_MemmyBackend_IsExecutableCommitted(function_region))
        {
            return 0;
        }
    }
    return 1;
}

static Memmy_Status Test_MemmyBackend_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                                     Memmy_ObjectBaseOptions const *options,
                                                     Memmy_ObjectBaseResult *out, Memmy_Error *error)
{
    Unused(arena);

    Test_MemmyBackend *backend = (Test_MemmyBackend *)process->backend_data;
    Test_MemmyBackendRegion *region = Test_MemmyBackend_FindRegion(backend, process->pid, address);
    if (!Test_MemmyBackend_IsReadableCommitted(region))
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("objectbase"),
                        String8_Lit("object base metadata not found"));
        return Memmy_Status_NotFound;
    }

    U64 pointer_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    Memmy_Addr scan_min = address > options->max_scan_back ? address - options->max_scan_back : 0;
    scan_min = Max(scan_min, region->base);
    Memmy_Addr candidate = address - (address % pointer_size);
    if (candidate > address)
    {
        candidate -= pointer_size;
    }

    B32 found = 0;
    Memmy_ObjectBaseResult best = {0};
    B32 ambiguous = 0;
    for (;;)
    {
        if (candidate < scan_min || candidate + pointer_size > region->base + region->size)
        {
            break;
        }

        Memmy_Addr vtable = 0;
        if (Test_MemmyBackend_ReadPointer(backend, process, candidate, &vtable) &&
            Test_MemmyBackend_IsPlausibleVtable(backend, process, vtable, options->min_vtable_entries))
        {
            Memmy_ObjectBaseResult result = {
                .address = candidate,
                .vptr_address = candidate,
                .vtable = vtable,
                .confidence = Memmy_ObjectBaseConfidence_Weak,
            };
            if (!found)
            {
                best = result;
                found = 1;
            }
            else if (best.confidence == result.confidence)
            {
                ambiguous = 1;
            }
        }

        if (candidate < pointer_size)
        {
            break;
        }
        candidate -= pointer_size;
    }

    if (!found)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("objectbase"),
                        String8_Lit("object base metadata not found"));
        return Memmy_Status_NotFound;
    }
    if (ambiguous)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("objectbase"),
                        String8_Lit("multiple object base candidates found"));
        return Memmy_Status_Ambiguous;
    }

    *out = best;
    return Memmy_Status_Ok;
}

void Test_MemmyBackend_Init(Test_MemmyBackend *backend)
{
    *backend = (Test_MemmyBackend){
        .backend =
            {
                .name = String8_Lit("test"),
                .enumerate_processes = Test_MemmyBackend_EnumerateProcesses,
                .open_process = Test_MemmyBackend_OpenProcess,
                .close_process = Test_MemmyBackend_CloseProcess,
                .read = Test_MemmyBackend_Read,
                .write = Test_MemmyBackend_Write,
                .enumerate_modules = Test_MemmyBackend_EnumerateModules,
                .enumerate_regions = Test_MemmyBackend_EnumerateRegions,
                .find_function = Test_MemmyBackend_FindFunction,
                .find_object_base = Test_MemmyBackend_FindObjectBase,
            },
        .memory_base = 0x1000,
        .read_status = Memmy_Status_Ok,
        .read_limit = 0,
        .read_call_count = 0,
        .open_call_count = 0,
        .close_call_count = 0,
        .last_open_pid = 0,
        .last_close_pid = 0,
        .min_read_addr = 0,
        .max_read_end = 0,
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

Test_MemmyBackendFunction *Test_MemmyBackend_AddFunction(Test_MemmyBackend *backend, U32 pid, Memmy_Addr start,
                                                         Memmy_Addr end)
{
    if (backend->function_count >= TEST_MEMMY_BACKEND_MAX_FUNCTIONS)
    {
        return 0;
    }

    Test_MemmyBackendFunction *function = &backend->functions[backend->function_count++];
    *function = (Test_MemmyBackendFunction){
        .pid = pid,
        .range = {.start = start, .end = end},
    };
    return function;
}

Test_MemmyBackendUnreadableRange *Test_MemmyBackend_AddUnreadableRange(Test_MemmyBackend *backend, Memmy_Addr start,
                                                                       Memmy_Addr end)
{
    if (backend->unreadable_range_count >= TEST_MEMMY_BACKEND_MAX_UNREADABLE_RANGES)
    {
        return 0;
    }

    Test_MemmyBackendUnreadableRange *range = &backend->unreadable_ranges[backend->unreadable_range_count++];
    *range = (Test_MemmyBackendUnreadableRange){
        .start = start,
        .end = end,
    };
    return range;
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
