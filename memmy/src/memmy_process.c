#include "memmy_process.h"

#include "memmy_context.h"
#include "memmy_range.h"

typedef struct Memmy_AccessibleRangeNode Memmy_AccessibleRangeNode;
struct Memmy_AccessibleRangeNode
{
    ListLink link;
    Memmy_Range range;
};

typedef struct Memmy_AccessibleRangeCollector Memmy_AccessibleRangeCollector;
struct Memmy_AccessibleRangeCollector
{
    Arena *arena;
    Memmy_Range bounds;
    Memmy_RegionAccess required_access;
    List ranges; // Memmy_AccessibleRangeNode
    Memmy_Error *error;
};

static I32 Memmy_AccessibleRange_Cmp(void *a, void *b, void *ctx)
{
    Unused(ctx);

    Memmy_Range *range_a = (Memmy_Range *)a;
    Memmy_Range *range_b = (Memmy_Range *)b;
    if (range_a->start < range_b->start)
    {
        return -1;
    }
    if (range_a->start > range_b->start)
    {
        return 1;
    }
    if (range_a->end < range_b->end)
    {
        return -1;
    }
    if (range_a->end > range_b->end)
    {
        return 1;
    }
    return 0;
}

static Memmy_Status Memmy_AccessibleRangeCollector_Push(void *user_data, Memmy_Region const *region)
{
    Memmy_AccessibleRangeCollector *collector = (Memmy_AccessibleRangeCollector *)user_data;
    if (region->state != Memmy_RegionState_Committed || (region->access & Memmy_RegionAccess_Guard) != 0 ||
        (region->access & collector->required_access) != collector->required_access)
    {
        return Memmy_Status_Ok;
    }

    Memmy_Addr region_end = 0;
    if (!AddU64Checked(region->base, region->size, &region_end))
    {
        Memmy_Error_Set(collector->error, Memmy_Status_Overflow, String8_Lit("region"),
                        String8_Lit("region end overflow"));
        return Memmy_Status_Overflow;
    }

    Memmy_Range intersection = {0};
    Memmy_Range region_range = {.start = region->base, .end = region_end};
    if (!Memmy_Range_Intersect(collector->bounds, region_range, &intersection))
    {
        return Memmy_Status_Ok;
    }

    Memmy_AccessibleRangeNode *node = Arena_PushStruct(collector->arena, Memmy_AccessibleRangeNode);
    node->range = intersection;
    List_PushBack(&collector->ranges, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_Status memmy_RequireContextBackend(Memmy_Backend **out_backend, Memmy_Error *error)
{
    Memmy_Context *ctx = Memmy_Context_Get();
    if (ctx == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing current context"));
        return Memmy_Status_InvalidArgument;
    }
    if (ctx->backend == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"), String8_Lit("missing backend"));
        return Memmy_Status_InvalidArgument;
    }

    *out_backend = ctx->backend;
    return Memmy_Status_Ok;
}

static Memmy_Status memmy_RequireProcessBackend(Memmy_Process *process, Memmy_Backend **out_backend, Memmy_Error *error)
{
    if (process == 0 || process->backend == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("process is not open"));
        return Memmy_Status_InvalidArgument;
    }

    *out_backend = process->backend;
    return Memmy_Status_Ok;
}

static Memmy_Status memmy_Unsupported(Memmy_Error *error, char *message)
{
    Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("backend"), String8_FromCStr(message));
    return Memmy_Status_Unsupported;
}

Memmy_Status Memmy_Process_Enumerate(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error)
{
    if (arena == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or process sink"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireContextBackend(&backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->enumerate_processes == 0)
    {
        return memmy_Unsupported(error, "backend cannot list processes");
    }

    return backend->enumerate_processes(arena, sink, error);
}

Memmy_Status Memmy_Process_Open(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = 0;
    }
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or process output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireContextBackend(&backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->open_process == 0)
    {
        return memmy_Unsupported(error, "backend cannot open processes");
    }

    return backend->open_process(arena, pid, out, error);
}

B32 Memmy_Process_IsOpen(Memmy_Process *process)
{
    return process != 0 && process->backend != 0;
}

void Memmy_Process_Close(Memmy_Process *process)
{
    if (process != 0 && process->backend != 0)
    {
        Memmy_Backend *backend = process->backend;
        if (backend->close_process != 0)
        {
            backend->close_process(process);
        }
        process->backend = 0;
    }
}

Memmy_Status Memmy_Process_EnumerateModules(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink,
                                            Memmy_Error *error)
{
    if (arena == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or module sink"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->enumerate_modules == 0)
    {
        return memmy_Unsupported(error, "backend cannot list modules");
    }

    return backend->enumerate_modules(arena, process, sink, error);
}

Memmy_Status Memmy_Process_EnumerateRegions(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink,
                                            Memmy_Error *error)
{
    if (arena == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or region sink"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->enumerate_regions == 0)
    {
        return memmy_Unsupported(error, "backend cannot list regions");
    }

    return backend->enumerate_regions(arena, process, sink, error);
}

Memmy_Status Memmy_Process_GetAddressRange(Memmy_Process *process, Memmy_Range *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Range){0};
    }
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing address range output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->get_address_range == 0)
    {
        return memmy_Unsupported(error, "backend cannot get the process address range");
    }

    Memmy_Range range = {0};
    status = backend->get_address_range(process, &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (range.start != 0 || range.end < range.start)
    {
        Memmy_Error_Set(error, Memmy_Status_PlatformError, String8_Lit("backend"),
                        String8_Lit("backend returned an invalid process address range"));
        return Memmy_Status_PlatformError;
    }

    *out = range;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Process_EnumerateAccessibleRanges(Arena *arena, Memmy_Process *process, Memmy_Range bounds,
                                                     Memmy_RegionAccess required_access, Memmy_RangeSink sink,
                                                     Memmy_Error *error)
{
    if (arena == 0 || process == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("region"),
                        String8_Lit("missing accessible-range argument"));
        return Memmy_Status_InvalidArgument;
    }
    if (bounds.end < bounds.start)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("region"),
                        String8_Lit("range end is before start"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    Unused(backend);

    if (Memmy_Range_IsEmpty(bounds))
    {
        return Memmy_Status_Ok;
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    Memmy_AccessibleRangeCollector collector = {
        .arena = scratch.arena,
        .bounds = bounds,
        .required_access = required_access,
        .error = error,
    };
    Memmy_RegionSink region_sink = {
        .callback = Memmy_AccessibleRangeCollector_Push,
        .user_data = &collector,
    };
    status = Memmy_Process_EnumerateRegions(scratch.arena, process, region_sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    Memmy_Range *ranges = Arena_PushArrayNoZero(scratch.arena, Memmy_Range, collector.ranges.count);
    U64 range_count = 0;
    List_ForEach(Memmy_AccessibleRangeNode, node, &collector.ranges, link)
    {
        ranges[range_count++] = node->range;
    }

    Sort(ranges, range_count, sizeof(ranges[0]), Memmy_AccessibleRange_Cmp, 0);
    for (U64 i = 0; i < range_count;)
    {
        Memmy_Range merged = ranges[i++];
        while (i < range_count && ranges[i].start <= merged.end)
        {
            merged.end = Max(merged.end, ranges[i].end);
            i++;
        }

        status = sink.callback(sink.user_data, merged);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
    }

    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Process_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Range *out,
                                        Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Range){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or function range output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->find_function == 0)
    {
        return memmy_Unsupported(error, "backend cannot find functions");
    }

    return backend->find_function(arena, process, address, out, error);
}

Memmy_Status Memmy_Process_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                          Memmy_ObjectBaseOptions const *options, Memmy_ObjectBaseResult *out,
                                          Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_ObjectBaseResult){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing output arena or object-base output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->find_object_base == 0)
    {
        return memmy_Unsupported(error, "backend cannot find object bases");
    }

    Memmy_ObjectBaseOptions normalized = options != 0 ? *options : (Memmy_ObjectBaseOptions){0};
    if (normalized.max_scan_back == 0)
    {
        normalized.max_scan_back = 0x1000;
    }
    if (normalized.min_vtable_entries == 0)
    {
        normalized.min_vtable_entries = 2;
    }

    return backend->find_object_base(arena, process, address, &normalized, out, error);
}

Memmy_Status Memmy_Process_Read(Memmy_Process *process, Memmy_Addr address, void *buffer, U64 size, U64 *bytes_read,
                                Memmy_Error *error)
{
    if (bytes_read != 0)
    {
        *bytes_read = 0;
    }
    if (buffer == 0 || bytes_read == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing read buffer or byte count output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->read == 0)
    {
        return memmy_Unsupported(error, "backend cannot read memory");
    }

    return backend->read(process, address, buffer, size, bytes_read, error);
}

Memmy_Status Memmy_Process_Write(Memmy_Process *process, Memmy_Addr address, void const *buffer, U64 size,
                                 U64 *bytes_written, Memmy_Error *error)
{
    if (bytes_written != 0)
    {
        *bytes_written = 0;
    }
    if (buffer == 0 || bytes_written == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"),
                        String8_Lit("missing write buffer or byte count output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Backend *backend = 0;
    Memmy_Status status = memmy_RequireProcessBackend(process, &backend, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (backend->write == 0)
    {
        return memmy_Unsupported(error, "backend cannot write memory");
    }

    return backend->write(process, address, buffer, size, bytes_written, error);
}
