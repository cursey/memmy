#include "memmy_process.h"

#include "memmy_context.h"

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

Memmy_Status Memmy_EnumerateProcesses(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error)
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

    *out = 0;
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

Memmy_Status Memmy_Process_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Range *out,
                                        Memmy_Error *error)
{
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

    *out = (Memmy_Range){0};
    return backend->find_function(arena, process, address, out, error);
}

Memmy_Status Memmy_Process_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                          Memmy_ObjectBaseOptions *options, Memmy_ObjectBaseResult *out,
                                          Memmy_Error *error)
{
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

    Memmy_ObjectBaseOptions defaults = {0};
    if (options == 0)
    {
        options = &defaults;
    }
    if (options->max_scan_back == 0)
    {
        options->max_scan_back = 0x1000;
    }
    if (options->min_vtable_entries == 0)
    {
        options->min_vtable_entries = 2;
    }

    *out = (Memmy_ObjectBaseResult){0};
    return backend->find_object_base(arena, process, address, options, out, error);
}

Memmy_Status Memmy_Process_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                                Memmy_Error *error)
{
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

    return backend->read(process, addr, buffer, size, bytes_read, error);
}

Memmy_Status Memmy_Process_Write(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_written,
                                 Memmy_Error *error)
{
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

    return backend->write(process, addr, buffer, size, bytes_written, error);
}
