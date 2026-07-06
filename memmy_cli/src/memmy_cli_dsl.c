#include "memmy_cli_internal.h"

typedef struct Memmy_CliProcessInfoResolver Memmy_CliProcessInfoResolver;
struct Memmy_CliProcessInfoResolver
{
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    Memmy_ProcessInfo match;
    U64 match_count;
};

typedef struct Memmy_CliEvalResultWriter Memmy_CliEvalResultWriter;
struct Memmy_CliEvalResultWriter
{
    Arena *arena;
    Memmy_EvalEnv *env;
    Memmy_CliOutputWriter writer;
    Memmy_Status status;
    B32 jsonl;
    B32 process_header_written;
    B32 module_header_written;
    B32 region_header_written;
    B32 suppress_value_result;
    B32 saw_exit;
    String8 assignment_name;
    Memmy_CliScanOutput scan_output;
};

static void Memmy_Cli_SetAstError(Memmy_Error *error, Memmy_Status status, Memmy_AstDiagnostic *diagnostic)
{
    Memmy_Error_Set(error, status, String8_Lit("expr"), diagnostic->message);
    if (error != 0)
    {
        error->input = diagnostic->input;
        error->byte_offset = diagnostic->byte_offset;
        error->byte_count = diagnostic->byte_count;
    }
}

static Memmy_Status Memmy_Cli_AstStatusToMemmyStatus(Memmy_AstStatus status)
{
    if (status == Memmy_AstStatus_Ok)
    {
        return Memmy_Status_Ok;
    }
    if (status == Memmy_AstStatus_Overflow)
    {
        return Memmy_Status_Overflow;
    }
    if (status == Memmy_AstStatus_InvalidArgument)
    {
        return Memmy_Status_InvalidArgument;
    }
    if (status == Memmy_AstStatus_Unsupported)
    {
        return Memmy_Status_Unsupported;
    }
    return Memmy_Status_ParseError;
}

static String8 Memmy_Cli_PointerWidthArchString(Memmy_PointerWidth pointer_width)
{
    if (pointer_width == Memmy_PointerWidth_32)
    {
        return String8_Lit("x86");
    }
    if (pointer_width == Memmy_PointerWidth_64)
    {
        return String8_Lit("x64");
    }
    return String8_Lit("?");
}

static Memmy_PointerWidth Memmy_CliEvalResultWriter_PointerWidth(Memmy_CliEvalResultWriter *result_writer)
{
    if (result_writer->env != 0 && result_writer->env->default_pointer_width != Memmy_PointerWidth_Unknown)
    {
        return result_writer->env->default_pointer_width;
    }
    return Memmy_PointerWidth_64;
}

static String8 Memmy_Cli_DslHelp(Arena *arena)
{
    return String8_Copy(arena, String8_Lit("Core values:\n"
                                           "  x                    constant integer/math expression\n"
                                           "  @x                   absolute address\n"
                                           "  [@a..@b]             explicit address range [a, b)\n"
                                           "  [@a..+n]             sized address range [a, a+n)\n"
                                           "  <module>             module range in selected process\n"
                                           "  [0..]                selected process readable regions\n"
                                           "  $name                variable\n"
                                           "\n"
                                           "Targets:\n"
                                           "  <client.dll>          module in selected process\n"
                                           "  Use /attach or --pid/--name to select a process\n"
                                           "\n"
                                           "Memory:\n"
                                           "  address as T         typed read\n"
                                           "  address as T = value typed write\n"
                                           "  range{pattern}       pattern scan -> address list\n"
                                           "  range as T == value  value scan -> address list\n"
                                           "  $name = expr         bind evaluated result\n"
                                           "  $name[N]             index address list\n"
                                           "\n"
                                           "Commands:\n"
                                           "  /procs [filter]\n"
                                           "  /attach <pid|name>\n"
                                           "  /detach\n"
                                           "  /mods [filter]\n"
                                           "  /regions\n"
                                           "  /vars\n"
                                           "  /unset $var\n"
                                           "  /clear\n"
                                           "  /help\n"
                                           "  /exit\n"
                                           "  /quit\n"));
}

static String8 Memmy_Cli_EvalValueKindString(Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_Const)
    {
        return String8_Lit("const");
    }
    if (value.kind == Memmy_EvalValueKind_Address)
    {
        return String8_Lit("address");
    }
    if (value.kind == Memmy_EvalValueKind_Range)
    {
        return String8_Lit("range");
    }
    if (value.kind == Memmy_EvalValueKind_ProcessRange)
    {
        return String8_Lit("process_range");
    }
    if (value.kind == Memmy_EvalValueKind_AddressList)
    {
        return String8_Lit("address_list");
    }
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        return String8_Lit("typed_value");
    }
    if (value.kind == Memmy_EvalValueKind_Null)
    {
        return String8_Lit("null");
    }
    return String8_Lit("unknown");
}

static Memmy_Status Memmy_CliProcessInfoResolver_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_CliProcessInfoResolver *resolver = (Memmy_CliProcessInfoResolver *)user_data;
    B32 matches = 0;
    if (resolver->has_pid)
    {
        matches = info->pid == resolver->pid;
    }
    else if (resolver->has_name)
    {
        matches = String8_EqNoCase(info->name, resolver->name);
    }

    if (matches)
    {
        resolver->match = *info;
        resolver->match_count++;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_ResolveProcessInfo(Arena *arena, B32 has_pid, U32 pid, B32 has_name, String8 name,
                                          Memmy_ProcessInfo *out, Memmy_Error *error)
{
    if ((!has_pid && !has_name) || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                        String8_Lit("missing process selector"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliProcessInfoResolver resolver = {
        .has_pid = has_pid,
        .pid = pid,
        .has_name = has_name,
        .name = name,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = Memmy_CliProcessInfoResolver_Push,
        .user_data = &resolver,
    };
    Memmy_Status status = Memmy_EnumerateProcesses(arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (resolver.match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("process"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (resolver.match_count > 1)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("process"),
                        String8_Lit("process name is ambiguous"));
        return Memmy_Status_Ambiguous;
    }

    *out = resolver.match;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_ResolvePidOrOpenTransient(Arena *arena, U32 pid, Memmy_ProcessInfo *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_ResolveProcessInfo(arena, 1, pid, 0, (String8){0}, out, error);
    if (status != Memmy_Status_Unsupported)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = (Memmy_ProcessInfo){
        .pid = pid,
        .pointer_width = process->pointer_width,
    };
    Memmy_Process_Close(process);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_SetOptionsProcess(Arena *arena, Memmy_CliOptions *options, Memmy_EvalEnv *env,
                                                Memmy_ProcessInfo *out_info, Memmy_Error *error)
{
    if (options == 0 || env == 0 || (!options->has_pid && !options->has_name))
    {
        return Memmy_Status_Ok;
    }

    U32 pid = options->pid;
    Memmy_ProcessInfo info = {0};
    if (options->has_pid)
    {
        Memmy_Status status = Memmy_Cli_ResolvePidOrOpenTransient(arena, pid, &info, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    else if (options->has_name)
    {
        Memmy_Status status = Memmy_Cli_ResolveProcessInfo(arena, 0, 0, 1, options->name, &info, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        pid = info.pid;
    }

    if (out_info != 0)
    {
        *out_info = info;
    }
    Memmy_EvalEnv_SetDefaultProcess(env, pid, info.pointer_width);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_CliEvalResultWriter_Write(Memmy_CliEvalResultWriter *result_writer, String8 text)
{
    if (result_writer->status != Memmy_Status_Ok)
    {
        return result_writer->status;
    }
    result_writer->status = result_writer->writer.write(result_writer->writer.user_data, text);
    return result_writer->status;
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteProcess(Memmy_CliEvalResultWriter *result_writer,
                                                           Memmy_ProcessInfo *info)
{
    Arena *arena = result_writer->arena;
    String8 arch = Memmy_Cli_PointerWidthArchString(info->pointer_width);
    if (result_writer->jsonl)
    {
        String8 name = Memmy_Cli_FormatJsonString(arena, info->name);
        String8 path = Memmy_Cli_FormatJsonString(arena, info->path);
        String8 line = String8_PushF(arena,
                                     "{\"type\":\"process\",\"pid\":%u,\"arch\":\"%.*s\",\"name\":%.*s,"
                                     "\"path\":%.*s}\n",
                                     info->pid, (int)arch.len, (char *)arch.data, (int)name.len, (char *)name.data,
                                     (int)path.len, (char *)path.data);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->process_header_written)
    {
        Memmy_Status status = Memmy_CliEvalResultWriter_Write(result_writer, String8_Lit("PID     ARCH   NAME\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->process_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%-7u %-5.*s %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                                 (int)info->name.len, (char *)info->name.data);
    return Memmy_CliEvalResultWriter_Write(result_writer, line);
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteModule(Memmy_CliEvalResultWriter *result_writer,
                                                          Memmy_Module *module)
{
    Arena *arena = result_writer->arena;
    String8 base = Memmy_Cli_FormatAddress(arena, Memmy_CliEvalResultWriter_PointerWidth(result_writer), module->base);
    if (result_writer->jsonl)
    {
        String8 name = Memmy_Cli_FormatJsonString(arena, module->name);
        String8 path = Memmy_Cli_FormatJsonString(arena, module->path);
        String8 line = String8_PushF(arena,
                                     "{\"type\":\"module\",\"base\":\"%.*s\",\"size\":%llu,\"name\":%.*s,"
                                     "\"path\":%.*s}\n",
                                     (int)base.len, (char *)base.data, (unsigned long long)module->size, (int)name.len,
                                     (char *)name.data, (int)path.len, (char *)path.data);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->module_header_written)
    {
        Memmy_Status status =
            Memmy_CliEvalResultWriter_Write(result_writer, String8_Lit("BASE               SIZE     NAME\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->module_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%.*s 0x%-6llx %.*s\n", (int)base.len, (char *)base.data,
                                 (unsigned long long)module->size, (int)module->name.len, (char *)module->name.data);
    return Memmy_CliEvalResultWriter_Write(result_writer, line);
}

static String8 Memmy_Cli_RegionAccessString(Arena *arena, Memmy_RegionAccess access)
{
    String8List parts = {0};
    if (access & Memmy_RegionAccess_Read)
    {
        String8List_Push(arena, &parts, String8_Lit("r"));
    }
    if (access & Memmy_RegionAccess_Write)
    {
        String8List_Push(arena, &parts, String8_Lit("w"));
    }
    if (access & Memmy_RegionAccess_Execute)
    {
        String8List_Push(arena, &parts, String8_Lit("x"));
    }
    if (access & Memmy_RegionAccess_Guard)
    {
        String8List_Push(arena, &parts, String8_Lit("g"));
    }
    if (parts.total_len == 0)
    {
        return String8_Lit("-");
    }
    return String8List_Join(arena, &parts, (String8){0});
}

static String8 Memmy_Cli_RegionStateString(Memmy_RegionState state)
{
    if (state == Memmy_RegionState_Committed)
    {
        return String8_Lit("committed");
    }
    if (state == Memmy_RegionState_Reserved)
    {
        return String8_Lit("reserved");
    }
    if (state == Memmy_RegionState_Free)
    {
        return String8_Lit("free");
    }
    return String8_Lit("unknown");
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteRegion(Memmy_CliEvalResultWriter *result_writer,
                                                          Memmy_Region *region)
{
    Arena *arena = result_writer->arena;
    String8 base = Memmy_Cli_FormatAddress(arena, Memmy_CliEvalResultWriter_PointerWidth(result_writer), region->base);
    String8 access = Memmy_Cli_RegionAccessString(arena, region->access);
    String8 state = Memmy_Cli_RegionStateString(region->state);
    if (result_writer->jsonl)
    {
        String8 access_json = Memmy_Cli_FormatJsonString(arena, access);
        String8 state_json = Memmy_Cli_FormatJsonString(arena, state);
        String8 line =
            String8_PushF(arena,
                          "{\"type\":\"region\",\"base\":\"%.*s\",\"size\":%llu,\"access\":%.*s,"
                          "\"state\":%.*s}\n",
                          (int)base.len, (char *)base.data, (unsigned long long)region->size, (int)access_json.len,
                          (char *)access_json.data, (int)state_json.len, (char *)state_json.data);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->region_header_written)
    {
        Memmy_Status status =
            Memmy_CliEvalResultWriter_Write(result_writer, String8_Lit("BASE               SIZE     ACCESS STATE\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->region_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%.*s 0x%-6llx %-6.*s %.*s\n", (int)base.len, (char *)base.data,
                                 (unsigned long long)region->size, (int)access.len, (char *)access.data, (int)state.len,
                                 (char *)state.data);
    return Memmy_CliEvalResultWriter_Write(result_writer, line);
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteValue(Memmy_CliEvalResultWriter *result_writer,
                                                         Memmy_EvalValue value)
{
    Arena *arena = result_writer->arena;
    Memmy_PointerWidth pointer_width = value.pointer_width != Memmy_PointerWidth_Unknown
                                           ? value.pointer_width
                                           : Memmy_CliEvalResultWriter_PointerWidth(result_writer);
    if (value.kind == Memmy_EvalValueKind_Null)
    {
        return Memmy_Status_Ok;
    }
    if (value.kind == Memmy_EvalValueKind_Const)
    {
        String8 line = result_writer->jsonl
                           ? String8_PushF(arena, "{\"type\":\"value\",\"kind\":\"const\",\"value\":%lld}\n",
                                           (long long)value.constant)
                           : String8_PushF(arena, "%lld\n", (long long)value.constant);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == Memmy_EvalValueKind_Address)
    {
        String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, value.address);
        String8 line = result_writer->jsonl ? String8_PushF(arena, "{\"type\":\"address\",\"address\":\"%.*s\"}\n",
                                                            (int)address.len, (char *)address.data)
                                            : String8_PushF(arena, "%.*s\n", (int)address.len, (char *)address.data);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == Memmy_EvalValueKind_Range)
    {
        String8 start = Memmy_Cli_FormatAddress(arena, pointer_width, value.range.start);
        String8 end = Memmy_Cli_FormatAddress(arena, pointer_width, value.range.end);
        String8 line = result_writer->jsonl
                           ? String8_PushF(arena, "{\"type\":\"range\",\"start\":\"%.*s\",\"end\":\"%.*s\"}\n",
                                           (int)start.len, (char *)start.data, (int)end.len, (char *)end.data)
                           : String8_PushF(arena, "[%.*s..%.*s)\n", (int)start.len, (char *)start.data, (int)end.len,
                                           (char *)end.data);
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == Memmy_EvalValueKind_ProcessRange)
    {
        String8 line = result_writer->jsonl
                           ? String8_Lit("{\"type\":\"process_range\",\"range\":\"readable_regions\"}\n")
                           : String8_Lit("[0..]\n");
        return Memmy_CliEvalResultWriter_Write(result_writer, line);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteAddressList(Memmy_CliEvalResultWriter *result_writer,
                                                               Memmy_EvalValue value)
{
    Memmy_Status status = Memmy_CliScanOutput_Begin(
        &result_writer->scan_output, result_writer->arena, result_writer->writer,
        value.pointer_width != Memmy_PointerWidth_Unknown ? value.pointer_width
                                                          : Memmy_CliEvalResultWriter_PointerWidth(result_writer),
        result_writer->jsonl);
    if (status != Memmy_Status_Ok)
    {
        result_writer->status = status;
        return status;
    }
    for (U64 i = 0; i < value.address_count; i++)
    {
        status = Memmy_CliScanOutput_PushMatch(&result_writer->scan_output, value.addresses[i]);
        if (status != Memmy_Status_Ok)
        {
            result_writer->status = status;
            return status;
        }
    }
    status = Memmy_CliScanOutput_End(&result_writer->scan_output);
    if (status != Memmy_Status_Ok)
    {
        result_writer->status = status;
    }
    return status;
}

static Memmy_Status Memmy_CliEvalResultWriter_WriteAssignment(Memmy_CliEvalResultWriter *result_writer,
                                                              Memmy_EvalValue value)
{
    if (!result_writer->jsonl)
    {
        return Memmy_Status_Ok;
    }

    Arena *arena = result_writer->arena;
    String8 name = Memmy_Cli_FormatJsonString(arena, result_writer->assignment_name);
    String8 kind = Memmy_Cli_EvalValueKindString(value);
    String8 line = String8_PushF(arena, "{\"type\":\"assignment\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
    return Memmy_CliEvalResultWriter_Write(result_writer, line);
}

static void Memmy_CliEvalResultWriter_Push(Memmy_EvalResultSink *sink, Memmy_EvalResult result)
{
    Memmy_CliEvalResultWriter *result_writer = (Memmy_CliEvalResultWriter *)sink->user_data;
    if (result_writer->status != Memmy_Status_Ok)
    {
        return;
    }

    Arena *arena = result_writer->arena;
    if (result_writer->suppress_value_result &&
        (result.kind == Memmy_EvalResultKind_Value || result.kind == Memmy_EvalResultKind_Read ||
         result.kind == Memmy_EvalResultKind_Write || result.kind == Memmy_EvalResultKind_AddressList))
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteAssignment(result_writer, result.value);
        return;
    }

    if (result.kind == Memmy_EvalResultKind_Value)
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteValue(result_writer, result.value);
    }
    else if (result.kind == Memmy_EvalResultKind_Read)
    {
        Memmy_CliPeekOutput peek = {
            .pointer_width = result.value.pointer_width != Memmy_PointerWidth_Unknown
                                 ? result.value.pointer_width
                                 : Memmy_CliEvalResultWriter_PointerWidth(result_writer),
            .address = result.address,
            .type = result.new_value.type,
            .type_text = Memmy_Cli_TypeString(result.new_value.type),
            .bytes = result.new_value.bytes,
        };
        String8 output = {0};
        result_writer->status = Memmy_Cli_FormatPeekOutput(arena, &peek, result_writer->jsonl, &output, 0);
        if (result_writer->status == Memmy_Status_Ok)
        {
            result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, output);
        }
    }
    else if (result.kind == Memmy_EvalResultKind_Write)
    {
        Memmy_CliPokeOutput poke = {
            .pid = result.value.has_process ? result.value.pid : 0,
            .pointer_width = result.value.pointer_width != Memmy_PointerWidth_Unknown
                                 ? result.value.pointer_width
                                 : Memmy_CliEvalResultWriter_PointerWidth(result_writer),
            .address = result.address,
            .type = result.new_value.type,
            .type_text = Memmy_Cli_TypeString(result.new_value.type),
            .old_bytes = result.old_value.bytes,
            .new_bytes = result.new_value.bytes,
            .dry_run = 0,
        };
        String8 output = {0};
        result_writer->status = Memmy_Cli_FormatPokeOutput(arena, &poke, result_writer->jsonl, &output, 0);
        if (result_writer->status == Memmy_Status_Ok)
        {
            result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, output);
        }
    }
    else if (result.kind == Memmy_EvalResultKind_AddressList)
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteAddressList(result_writer, result.value);
    }
    else if (result.kind == Memmy_EvalResultKind_Process)
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteProcess(result_writer, &result.process);
    }
    else if (result.kind == Memmy_EvalResultKind_Module)
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteModule(result_writer, &result.module);
    }
    else if (result.kind == Memmy_EvalResultKind_Region)
    {
        result_writer->status = Memmy_CliEvalResultWriter_WriteRegion(result_writer, &result.region);
    }
    else if (result.kind == Memmy_EvalResultKind_Variable)
    {
        String8 kind = Memmy_Cli_EvalValueKindString(result.variable.value);
        String8 line = {0};
        if (result_writer->jsonl)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, result.variable.name);
            line = String8_PushF(arena, "{\"type\":\"variable\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
        }
        else
        {
            line = String8_PushF(arena, "%.*s %.*s\n", (int)result.variable.name.len, (char *)result.variable.name.data,
                                 (int)kind.len, (char *)kind.data);
        }
        result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, line);
    }
    else if (result.kind == Memmy_EvalResultKind_Unset)
    {
        if (result_writer->jsonl)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, result.name);
            String8 line =
                String8_PushF(arena, "{\"type\":\"unset\",\"name\":%.*s}\n", (int)name.len, (char *)name.data);
            result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, line);
        }
    }
    else if (result.kind == Memmy_EvalResultKind_Clear)
    {
        if (result_writer->jsonl)
        {
            result_writer->status =
                Memmy_CliEvalResultWriter_Write(result_writer, String8_Lit("{\"type\":\"clear\"}\n"));
        }
    }
    else if (result.kind == Memmy_EvalResultKind_Help)
    {
        String8 help = Memmy_Cli_DslHelp(arena);
        if (result_writer->jsonl)
        {
            String8 text = Memmy_Cli_FormatJsonString(arena, help);
            String8 line =
                String8_PushF(arena, "{\"type\":\"help\",\"text\":%.*s}\n", (int)text.len, (char *)text.data);
            result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, line);
        }
        else
        {
            result_writer->status = Memmy_CliEvalResultWriter_Write(result_writer, help);
        }
    }
    else if (result.kind == Memmy_EvalResultKind_Exit)
    {
        result_writer->saw_exit = 1;
    }
}

Memmy_Status Memmy_Cli_RunExprToWriter(Arena *arena, Memmy_CliOptions *options, Memmy_CliOutputWriter writer,
                                       Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    return Memmy_Cli_RunExprToWriterWithEnv(arena, env, options, writer, error);
}

Memmy_Status Memmy_Cli_RunExprToWriterWithEnv(Arena *arena, Memmy_EvalEnv *env, Memmy_CliOptions *options,
                                              Memmy_CliOutputWriter writer, Memmy_Error *error)
{
    if (options == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing options"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_Cli_RunStatementToWriterWithEnv(arena, env, options, options->expr_text, writer, 0, error);
}

Memmy_Status Memmy_Cli_RunStatementToWriterWithEnv(Arena *arena, Memmy_EvalEnv *env, Memmy_CliOptions *options,
                                                   String8 text, Memmy_CliOutputWriter writer, B32 *out_exit,
                                                   Memmy_Error *error)
{
    if (arena == 0 || env == 0 || options == 0 || writer.write == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output writer"));
        return Memmy_Status_InvalidArgument;
    }

    if (out_exit != 0)
    {
        *out_exit = 0;
    }

    Memmy_AstStatement statement = {0};
    Memmy_AstDiagnostic diagnostic = {0};
    Memmy_AstStatus ast_status = Memmy_Ast_ParseStatement(arena, text, &statement, &diagnostic);
    if (ast_status != Memmy_AstStatus_Ok)
    {
        Memmy_Status status = Memmy_Cli_AstStatusToMemmyStatus(ast_status);
        Memmy_Cli_SetAstError(error, status, &diagnostic);
        return status;
    }
    if (statement.kind == Memmy_AstNodeKind_Command && (statement.command_kind == Memmy_AstCommandKind_Attach ||
                                                        statement.command_kind == Memmy_AstCommandKind_Detach))
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"),
                        String8_Lit("command is only available in a REPL session"));
        return Memmy_Status_InvalidArgument;
    }

    B32 previous_has_default_process = env->has_default_process;
    U32 previous_default_pid = env->default_pid;
    Memmy_PointerWidth previous_default_pointer_width = env->default_pointer_width;

    Memmy_Status status = Memmy_Cli_SetOptionsProcess(arena, options, env, 0, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_CliEvalResultWriter result_writer = {
        .arena = arena,
        .env = env,
        .writer = writer,
        .status = Memmy_Status_Ok,
        .jsonl = options->jsonl,
        .suppress_value_result = statement.kind == Memmy_AstNodeKind_Assignment,
        .assignment_name = statement.assignment_name,
    };
    Memmy_EvalResultSink sink = {
        .push = Memmy_CliEvalResultWriter_Push,
        .user_data = &result_writer,
    };
    status = Memmy_EvalStatement(env, &statement, &sink, error);
    if (out_exit != 0)
    {
        *out_exit = result_writer.saw_exit;
    }

    if (previous_has_default_process)
    {
        Memmy_EvalEnv_SetDefaultProcess(env, previous_default_pid, previous_default_pointer_width);
    }
    else
    {
        Memmy_EvalEnv_ClearDefaultProcess(env);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return result_writer.status;
}
