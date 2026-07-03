#include "memmy_cli.h"

#include <stdarg.h>

#include "base_checked.h"

static void Memmy_Cli_SetRangeError(Memmy_Error *error, String8 message)
{
    Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("range"), message);
}

static B32 Memmy_Cli_IsOption(char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-';
}

static Memmy_Status Memmy_Cli_ReadOptionValue(I32 argc, char **argv, I32 index, String8 option, String8 *out,
                                              Memmy_Error *error)
{
    if (index + 1 >= argc || Memmy_Cli_IsOption(argv[index + 1]))
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("range"),
                        String8_Lit("missing range option value"));
        if (error != 0)
        {
            error->input = option;
        }
        return Memmy_Status_ParseError;
    }

    *out = String8_FromCStr(argv[index + 1]);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_ParseRangeOptions(I32 argc, char **argv, Memmy_Range *out, Memmy_Error *error)
{
    String8 start_text = {0};
    String8 end_text = {0};
    String8 length_text = {0};
    B32 has_start = 0;
    B32 has_end = 0;
    B32 has_length = 0;

    for (I32 i = 0; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, String8_Lit("--start")))
        {
            if (has_start)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --start"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &start_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_start = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--end")))
        {
            if (has_end)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --end"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &end_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_end = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--length")))
        {
            if (has_length)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --length"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &length_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_length = 1;
            i++;
        }
    }

    if (!has_start)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("missing --start"));
        return Memmy_Status_ParseError;
    }
    if (has_end && has_length)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("use --end or --length, not both"));
        return Memmy_Status_ParseError;
    }
    if (!has_end && !has_length)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("missing --end or --length"));
        return Memmy_Status_ParseError;
    }

    Memmy_Addr start = 0;
    Memmy_Status status = Memmy_ParseAddress(start_text, &start, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (has_end)
    {
        Memmy_Addr end = 0;
        status = Memmy_ParseAddress(end_text, &end, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartEnd(start, end, out, error);
    }

    Memmy_Size length = 0;
    status = Memmy_ParseSize(length_text, &length, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return Memmy_Range_FromStartLength(start, length, out, error);
}

typedef struct Memmy_CliOptions Memmy_CliOptions;
struct Memmy_CliOptions
{
    String8 command;
    B32 help;
    B32 version;
    B32 json;
    B32 jsonl;
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    B32 has_filter;
    String8 filter;
};

static void Memmy_Cli_PushLine(Arena *arena, String8List *list, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 line = String8_PushFV(arena, fmt, args);
    va_end(args);
    String8List_Push(arena, list, line);
}

static B32 Memmy_Cli_ContainsNoCase(String8 text, String8 needle)
{
    if (needle.len == 0)
    {
        return 1;
    }
    if (needle.len > text.len)
    {
        return 0;
    }

    for (U64 i = 0; i <= text.len - needle.len; i++)
    {
        if (String8_EqNoCase(String8_Substr(text, i, needle.len), needle))
        {
            return 1;
        }
    }
    return 0;
}

static String8 Memmy_Cli_PointerWidthString(Memmy_PointerWidth width)
{
    String8 result = String8_Lit("?");
    if (width == Memmy_PointerWidth_32)
    {
        result = String8_Lit("x86");
    }
    else if (width == Memmy_PointerWidth_64)
    {
        result = String8_Lit("x64");
    }
    return result;
}

static String8 Memmy_Cli_RegionAccessString(Memmy_RegionAccess access)
{
    if (access & Memmy_RegionAccess_Guard)
    {
        return String8_Lit("g--");
    }

    static U8 buffer[4];
    buffer[0] = (access & Memmy_RegionAccess_Read) ? 'r' : '-';
    buffer[1] = (access & Memmy_RegionAccess_Write) ? 'w' : '-';
    buffer[2] = (access & Memmy_RegionAccess_Execute) ? 'x' : '-';
    buffer[3] = 0;
    return String8_Make(buffer, 3);
}

static String8 Memmy_Cli_RegionStateString(Memmy_RegionState state)
{
    String8 result = String8_Lit("free");
    if (state == Memmy_RegionState_Reserved)
    {
        result = String8_Lit("reserved");
    }
    else if (state == Memmy_RegionState_Committed)
    {
        result = String8_Lit("committed");
    }
    return result;
}

static Memmy_Status Memmy_Cli_ParsePid(String8 text, U32 *out, Memmy_Error *error)
{
    Memmy_Size value = 0;
    Memmy_Status status = Memmy_ParseSize(text, &value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (value > U32_MAX)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("pid"), String8_Lit("pid overflow"));
        return Memmy_Status_Overflow;
    }

    *out = (U32)value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ParseOptions(I32 argc, char **argv, Memmy_CliOptions *out, Memmy_Error *error)
{
    *out = (Memmy_CliOptions){0};
    for (I32 i = 1; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, String8_Lit("--help")))
        {
            out->help = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--version")))
        {
            out->version = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--json")))
        {
            out->json = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--jsonl")))
        {
            out->jsonl = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--pid")))
        {
            if (out->has_pid)
            {
                Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("duplicate --pid"));
                return Memmy_Status_ParseError;
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Cli_ParsePid(value, &out->pid, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_pid = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--name")))
        {
            if (out->has_name)
            {
                Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("duplicate --name"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->name, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_name = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--filter")))
        {
            if (out->has_filter)
            {
                Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("duplicate --filter"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->filter, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_filter = 1;
            i++;
        }
        else if (Memmy_Cli_IsOption(argv[i]))
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("unknown option"));
            if (error != 0)
            {
                error->input = arg;
            }
            return Memmy_Status_ParseError;
        }
        else if (out->command.len == 0)
        {
            out->command = arg;
        }
        else
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("unexpected argument"));
            if (error != 0)
            {
                error->input = arg;
            }
            return Memmy_Status_ParseError;
        }
    }

    if (out->json || out->jsonl)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("cli"), String8_Lit("json output is not ready"));
        return Memmy_Status_Unsupported;
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RequireCap(Memmy_BackendCap cap, Memmy_Error *error)
{
    Memmy_Context *ctx = Memmy_Context_Get();
    if (ctx == 0 || ctx->backend == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"), String8_Lit("missing backend"));
        return Memmy_Status_InvalidArgument;
    }
    if (!Memmy_Backend_HasCapability(ctx->backend, cap))
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("backend"),
                        String8_Lit("backend capability is unavailable"));
        return Memmy_Status_Unsupported;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ResolveTarget(Arena *arena, Memmy_CliOptions *options, U32 *out_pid, Memmy_Error *error)
{
    if (options->has_pid == options->has_name)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("use exactly one target"));
        return Memmy_Status_ParseError;
    }

    if (options->has_pid)
    {
        *out_pid = options->pid;
        return Memmy_Status_Ok;
    }

    Memmy_ProcessList processes = {0};
    Memmy_Status status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U32 match_pid = 0;
    U64 match_count = 0;
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (String8_EqNoCase(info->name, options->name))
        {
            match_pid = info->pid;
            match_count++;
        }
    }

    if (match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("process"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (match_count > 1)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("process"),
                        String8_Lit("process name is ambiguous"));
        return Memmy_Status_Ambiguous;
    }

    *out_pid = match_pid;
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_Help(Arena *arena)
{
    return String8_Copy(arena, String8_Lit("memmy [global-options] <command> [command-options]\n"
                                           "\n"
                                           "Commands:\n"
                                           "  procs\n"
                                           "  mods --pid <pid>\n"
                                           "  regions --pid <pid>\n"
                                           "\n"
                                           "Global options:\n"
                                           "  --help\n"
                                           "  --version\n"));
}

static Memmy_Status Memmy_Cli_RunProcs(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListProcs, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ProcessList processes = {0};
    status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8List lines = {0};
    String8List_Push(arena, &lines, String8_Lit("PID     ARCH   NAME\n"));
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (options->has_filter && !Memmy_Cli_ContainsNoCase(info->name, options->filter) &&
            !Memmy_Cli_ContainsNoCase(info->path, options->filter))
        {
            continue;
        }

        String8 arch = Memmy_Cli_PointerWidthString(info->pointer_width);
        Memmy_Cli_PushLine(arena, &lines, "%u    %.*s    %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                           (int)info->name.len, (char *)info->name.data);
    }
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RunMods(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListModules, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Query, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ModuleList modules = {0};
    status = Memmy_Process_ListModules(arena, process, &modules, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    String8List lines = {0};
    String8List_Push(arena, &lines, String8_Lit("BASE                SIZE        NAME\n"));
    List_ForEach(Memmy_Module, module, &modules.list, link)
    {
        if (options->has_filter && !Memmy_Cli_ContainsNoCase(module->name, options->filter) &&
            !Memmy_Cli_ContainsNoCase(module->path, options->filter))
        {
            continue;
        }

        Memmy_Cli_PushLine(arena, &lines, "0x%016llx  0x%llx    %.*s\n", (unsigned long long)module->base,
                           (unsigned long long)module->size, (int)module->name.len, (char *)module->name.data);
    }

    Memmy_Process_Close(process);
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RunRegions(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListRegions, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Query, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_RegionList regions = {0};
    status = Memmy_Process_ListRegions(arena, process, &regions, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    String8List lines = {0};
    String8List_Push(arena, &lines, String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"));
    List_ForEach(Memmy_Region, region, &regions.list, link)
    {
        Memmy_Addr end = 0;
        if (!AddU64Checked(region->base, region->size, &end))
        {
            Memmy_Process_Close(process);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("region"), String8_Lit("region end overflow"));
            return Memmy_Status_Overflow;
        }

        String8 access = Memmy_Cli_RegionAccessString(region->access);
        String8 state = Memmy_Cli_RegionStateString(region->state);
        Memmy_Cli_PushLine(arena, &lines, "0x%016llx  0x%016llx  0x%llx     %.*s     %.*s\n",
                           (unsigned long long)region->base, (unsigned long long)end, (unsigned long long)region->size,
                           (int)access.len, (char *)access.data, (int)state.len, (char *)state.data);
    }

    Memmy_Process_Close(process);
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    Memmy_CliOptions options = {0};
    Memmy_Status status = Memmy_Cli_ParseOptions(argc, argv, &options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (options.version)
    {
        *out = String8_Copy(arena, String8_Lit("memmy 0.0.0\n"));
        return Memmy_Status_Ok;
    }
    if (options.help || options.command.len == 0)
    {
        *out = Memmy_Cli_Help(arena);
        return Memmy_Status_Ok;
    }

    if (String8_Eq(options.command, String8_Lit("procs")))
    {
        return Memmy_Cli_RunProcs(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("mods")))
    {
        return Memmy_Cli_RunMods(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("regions")))
    {
        return Memmy_Cli_RunRegions(arena, &options, out, error);
    }

    Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("unknown command"));
    if (error != 0)
    {
        error->input = options.command;
    }
    return Memmy_Status_ParseError;
}

I32 Memmy_Cli_ExitCodeFromStatus(Memmy_Status status)
{
    I32 result = 1;
    switch (status)
    {
    case Memmy_Status_Ok:
        result = 0;
        break;
    case Memmy_Status_InvalidArgument:
    case Memmy_Status_ParseError:
    case Memmy_Status_Overflow:
    case Memmy_Status_InvalidEncoding:
        result = 2;
        break;
    case Memmy_Status_NotFound:
    case Memmy_Status_Ambiguous:
        result = 3;
        break;
    case Memmy_Status_AccessDenied:
        result = 4;
        break;
    case Memmy_Status_PartialRead:
    case Memmy_Status_PartialWrite:
        result = 5;
        break;
    case Memmy_Status_Unsupported:
        result = 6;
        break;
    case Memmy_Status_PlatformError:
        result = 7;
        break;
    case Memmy_Status_Unreadable:
    case Memmy_Status_Unwritable:
    case Memmy_Status_OutOfMemory:
        result = 1;
        break;
    }
    return result;
}
