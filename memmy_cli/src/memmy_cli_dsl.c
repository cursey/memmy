#include "memmy_cli_internal.h"

typedef struct MemmyCli_ProcessInfoResolver MemmyCli_ProcessInfoResolver;
struct MemmyCli_ProcessInfoResolver
{
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    Memmy_ProcessInfo match;
    U64 match_count;
};

typedef struct MemmyCli_EvalResultWriter MemmyCli_EvalResultWriter;
struct MemmyCli_EvalResultWriter
{
    Arena *arena;
    MemmyEval_Env *env;
    MemmyCli_OutputWriter writer;
    Memmy_Status status;
    B32 jsonl;
    B32 process_header_written;
    B32 module_header_written;
    B32 region_header_written;
    B32 suppress_value_result;
    B32 saw_exit;
    String8 assignment_name;
    MemmyEval_ResultSink const *observer;
    MemmyCli_ScanOutput scan_output;
};

static void MemmyCli_AstError_Set(Memmy_Error *error, Memmy_Status status, MemmyAst_Diagnostic *diagnostic)
{
    Memmy_Error_Set(error, status, String8_Lit("expr"), diagnostic->message);
    if (error != 0)
    {
        error->input = diagnostic->input;
        error->byte_offset = diagnostic->byte_offset;
        error->byte_count = diagnostic->byte_count;
    }
}

static Memmy_Status MemmyCli_AstStatus_ToMemmyStatus(MemmyAst_Status status)
{
    if (status == MemmyAst_Status_Ok)
    {
        return Memmy_Status_Ok;
    }
    if (status == MemmyAst_Status_Overflow)
    {
        return Memmy_Status_Overflow;
    }
    if (status == MemmyAst_Status_InvalidArgument)
    {
        return Memmy_Status_InvalidArgument;
    }
    if (status == MemmyAst_Status_Unsupported)
    {
        return Memmy_Status_Unsupported;
    }
    return Memmy_Status_ParseError;
}

static String8 MemmyCli_PointerWidth_ArchString(Memmy_PointerWidth pointer_width)
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

static Memmy_PointerWidth MemmyCli_EvalResultWriter_PointerWidth(MemmyCli_EvalResultWriter *result_writer)
{
    Memmy_PointerWidth pointer_width = Memmy_PointerWidth_Unknown;
    if (MemmyEval_Env_GetDefaultProcess(result_writer->env, 0, &pointer_width))
    {
        return pointer_width;
    }
    return Memmy_PointerWidth_64;
}

static U32 MemmyCli_EvalResultWriter_Pid(MemmyCli_EvalResultWriter *result_writer)
{
    U32 pid = 0;
    MemmyEval_Env_GetDefaultProcess(result_writer->env, &pid, 0);
    return pid;
}

static String8 MemmyCli_Dsl_Help(Arena *arena)
{
    return String8_Copy(arena, String8_Lit("Core values:\n"
                                           "  x                    constant integer/math expression\n"
                                           "  nil                  type-neutral absence of a value\n"
                                           "  @x                   absolute address\n"
                                           "  [@a..@b]             explicit address range [a, b)\n"
                                           "  [@a..+n]             sized address range [a, a+n)\n"
                                           "  <module>             module range in selected process\n"
                                           "  [0..]                selected process readable regions\n"
                                           "  function address     function range containing address\n"
                                           "  objectbase address   best-effort object base containing address\n"
                                           "  $name                variable\n"
                                           "  $rva = $hit - <module>  module-relative offset const\n"
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
                                           "  range refs ptr addr  pointer reference scan -> address list\n"
                                           "  range refs rel32 addr rel32 reference scan -> address list\n"
                                           "  range refs any addr  ptr or rel32 reference scan -> address list\n"
                                           "  range disasm x64 {...} x64 disassembly scan -> address list\n"
                                           "  value |> expr        bind value to $ and evaluate expr once\n"
                                           "  list => expr         filter-map address/range items\n"
                                           "                       failed and nil RHS results are omitted\n"
                                           "  $                    current flow input inside a flow RHS\n"
                                           "  Flows chain left-to-right; parentheses nest; inner $ shadows outer $\n"
                                           "  $matches |> $[0]\n"
                                           "  $matches => [$..+0x20]\n"
                                           "  $xrefs => function $\n"
                                           "  $hits => objectbase $\n"
                                           "  $ranges => $ + 4\n"
                                           "  $name = expr         bind evaluated result\n"
                                           "  $name[N]             index address/range list\n"
                                           "\n"
                                           "Commands:\n"
                                           "  /procs [filter]\n"
                                           "  /attach <pid|name>  select process and clear variables\n"
                                           "  /detach             clear selected process and variables\n"
                                           "  /mods [filter]\n"
                                           "  /regions\n"
                                           "  /vars\n"
                                           "  /unset $var\n"
                                           "  /clear\n"
                                           "  /help\n"
                                           "  /tutorial [hint|restart|stop]\n"
                                           "  /exit\n"
                                           "  /quit\n"));
}

static String8 MemmyCli_EvalValue_KindString(MemmyEval_Value value)
{
    if (value.kind == MemmyEval_ValueKind_Const)
    {
        return String8_Lit("const");
    }
    if (value.kind == MemmyEval_ValueKind_Address)
    {
        return String8_Lit("address");
    }
    if (value.kind == MemmyEval_ValueKind_Range)
    {
        return String8_Lit("range");
    }
    if (value.kind == MemmyEval_ValueKind_ProcessRange)
    {
        return String8_Lit("process_range");
    }
    if (value.kind == MemmyEval_ValueKind_AddressList)
    {
        return String8_Lit("address_list");
    }
    if (value.kind == MemmyEval_ValueKind_RangeList)
    {
        return String8_Lit("range_list");
    }
    if (value.kind == MemmyEval_ValueKind_TypedValue)
    {
        return String8_Lit("typed_value");
    }
    if (value.kind == MemmyEval_ValueKind_Nil)
    {
        return String8_Lit("nil");
    }
    return String8_Lit("unknown");
}

static Memmy_Status MemmyCli_ProcessInfoResolver_Push(void *user_data, Memmy_ProcessInfo const *info)
{
    MemmyCli_ProcessInfoResolver *resolver = (MemmyCli_ProcessInfoResolver *)user_data;
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

Memmy_Status MemmyCli_ProcessInfo_Resolve(Arena *arena, B32 has_pid, U32 pid, B32 has_name, String8 name,
                                          Memmy_ProcessInfo *out, Memmy_Error *error)
{
    if ((!has_pid && !has_name) || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                        String8_Lit("missing process selector"));
        return Memmy_Status_InvalidArgument;
    }

    MemmyCli_ProcessInfoResolver resolver = {
        .has_pid = has_pid,
        .pid = pid,
        .has_name = has_name,
        .name = name,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = MemmyCli_ProcessInfoResolver_Push,
        .user_data = &resolver,
    };
    Memmy_Status status = Memmy_Process_Enumerate(arena, sink, error);
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

Memmy_Status MemmyCli_Pid_ResolveOrOpenTransient(Arena *arena, U32 pid, Memmy_ProcessInfo *out, Memmy_Error *error)
{
    Memmy_Status status = MemmyCli_ProcessInfo_Resolve(arena, 1, pid, 0, (String8){0}, out, error);
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

static Memmy_Status MemmyCli_Options_SetProcess(Arena *arena, MemmyCli_Options *options, MemmyEval_Env *env,
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
        Memmy_Status status = MemmyCli_Pid_ResolveOrOpenTransient(arena, pid, &info, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    else if (options->has_name)
    {
        Memmy_Status status = MemmyCli_ProcessInfo_Resolve(arena, 0, 0, 1, options->name, &info, error);
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
    MemmyEval_Env_SetDefaultProcess(env, pid, info.pointer_width);
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyCli_EvalResultWriter_Write(MemmyCli_EvalResultWriter *result_writer, String8 text)
{
    if (result_writer->status != Memmy_Status_Ok)
    {
        return result_writer->status;
    }
    result_writer->status = result_writer->writer.write(result_writer->writer.user_data, text);
    return result_writer->status;
}

static Memmy_Status MemmyCli_EvalResultWriter_WriteProcess(MemmyCli_EvalResultWriter *result_writer,
                                                           Memmy_ProcessInfo *info)
{
    Arena *arena = result_writer->arena;
    String8 arch = MemmyCli_PointerWidth_ArchString(info->pointer_width);
    if (result_writer->jsonl)
    {
        String8 name = MemmyCli_JsonString_Format(arena, info->name);
        String8 path = MemmyCli_JsonString_Format(arena, info->path);
        String8 line = String8_PushF(arena,
                                     "{\"type\":\"process\",\"pid\":%u,\"arch\":\"%.*s\",\"name\":%.*s,"
                                     "\"path\":%.*s}\n",
                                     info->pid, (int)arch.len, (char *)arch.data, (int)name.len, (char *)name.data,
                                     (int)path.len, (char *)path.data);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->process_header_written)
    {
        Memmy_Status status = MemmyCli_EvalResultWriter_Write(result_writer, String8_Lit("PID     ARCH   NAME\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->process_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%-7u %-5.*s %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                                 (int)info->name.len, (char *)info->name.data);
    return MemmyCli_EvalResultWriter_Write(result_writer, line);
}

static Memmy_Status MemmyCli_EvalResultWriter_WriteModule(MemmyCli_EvalResultWriter *result_writer,
                                                          Memmy_Module *module)
{
    Arena *arena = result_writer->arena;
    String8 base = MemmyCli_Address_Format(arena, MemmyCli_EvalResultWriter_PointerWidth(result_writer), module->base);
    if (result_writer->jsonl)
    {
        String8 name = MemmyCli_JsonString_Format(arena, module->name);
        String8 path = MemmyCli_JsonString_Format(arena, module->path);
        String8 line = String8_PushF(arena,
                                     "{\"type\":\"module\",\"base\":\"%.*s\",\"size\":%llu,\"name\":%.*s,"
                                     "\"path\":%.*s}\n",
                                     (int)base.len, (char *)base.data, (unsigned long long)module->size, (int)name.len,
                                     (char *)name.data, (int)path.len, (char *)path.data);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->module_header_written)
    {
        Memmy_Status status =
            MemmyCli_EvalResultWriter_Write(result_writer, String8_Lit("BASE               SIZE     NAME\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->module_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%.*s 0x%-6llx %.*s\n", (int)base.len, (char *)base.data,
                                 (unsigned long long)module->size, (int)module->name.len, (char *)module->name.data);
    return MemmyCli_EvalResultWriter_Write(result_writer, line);
}

static String8 MemmyCli_RegionAccess_String(Arena *arena, Memmy_RegionAccess access)
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

static String8 MemmyCli_RegionState_String(Memmy_RegionState state)
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

static Memmy_Status MemmyCli_EvalResultWriter_WriteRegion(MemmyCli_EvalResultWriter *result_writer,
                                                          Memmy_Region *region)
{
    Arena *arena = result_writer->arena;
    String8 base = MemmyCli_Address_Format(arena, MemmyCli_EvalResultWriter_PointerWidth(result_writer), region->base);
    String8 access = MemmyCli_RegionAccess_String(arena, region->access);
    String8 state = MemmyCli_RegionState_String(region->state);
    if (result_writer->jsonl)
    {
        String8 access_json = MemmyCli_JsonString_Format(arena, access);
        String8 state_json = MemmyCli_JsonString_Format(arena, state);
        String8 line =
            String8_PushF(arena,
                          "{\"type\":\"region\",\"base\":\"%.*s\",\"size\":%llu,\"access\":%.*s,"
                          "\"state\":%.*s}\n",
                          (int)base.len, (char *)base.data, (unsigned long long)region->size, (int)access_json.len,
                          (char *)access_json.data, (int)state_json.len, (char *)state_json.data);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }

    if (!result_writer->region_header_written)
    {
        Memmy_Status status =
            MemmyCli_EvalResultWriter_Write(result_writer, String8_Lit("BASE               SIZE     ACCESS STATE\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result_writer->region_header_written = 1;
    }
    String8 line = String8_PushF(arena, "%.*s 0x%-6llx %-6.*s %.*s\n", (int)base.len, (char *)base.data,
                                 (unsigned long long)region->size, (int)access.len, (char *)access.data, (int)state.len,
                                 (char *)state.data);
    return MemmyCli_EvalResultWriter_Write(result_writer, line);
}

static Memmy_Status MemmyCli_EvalResultWriter_WriteValue(MemmyCli_EvalResultWriter *result_writer,
                                                         MemmyEval_Value value)
{
    Arena *arena = result_writer->arena;
    Memmy_PointerWidth pointer_width = MemmyCli_EvalResultWriter_PointerWidth(result_writer);
    if (value.kind == MemmyEval_ValueKind_Nil)
    {
        String8 line = result_writer->jsonl ? String8_Lit("{\"type\":\"value\",\"kind\":\"nil\",\"value\":null}\n")
                                            : String8_Lit("nil\n");
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == MemmyEval_ValueKind_Const)
    {
        String8 line = result_writer->jsonl
                           ? String8_PushF(arena,
                                           "{\"type\":\"value\",\"kind\":\"const\",\"value_kind\":\"const\","
                                           "\"value\":%lld}\n",
                                           (long long)value.constant)
                           : String8_PushF(arena, "%lld\n", (long long)value.constant);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == MemmyEval_ValueKind_Address)
    {
        String8 address = MemmyCli_Address_Format(arena, pointer_width, value.address);
        String8 line = result_writer->jsonl ? String8_PushF(arena, "{\"type\":\"address\",\"address\":\"%.*s\"}\n",
                                                            (int)address.len, (char *)address.data)
                                            : String8_PushF(arena, "%.*s\n", (int)address.len, (char *)address.data);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == MemmyEval_ValueKind_Range)
    {
        String8 start = MemmyCli_Address_Format(arena, pointer_width, value.range.start);
        String8 end = MemmyCli_Address_Format(arena, pointer_width, value.range.end);
        String8 line = result_writer->jsonl
                           ? String8_PushF(arena, "{\"type\":\"range\",\"start\":\"%.*s\",\"end\":\"%.*s\"}\n",
                                           (int)start.len, (char *)start.data, (int)end.len, (char *)end.data)
                           : String8_PushF(arena, "[%.*s..%.*s)\n", (int)start.len, (char *)start.data, (int)end.len,
                                           (char *)end.data);
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == MemmyEval_ValueKind_ProcessRange)
    {
        String8 line = result_writer->jsonl
                           ? String8_Lit("{\"type\":\"process_range\",\"range\":\"readable_regions\"}\n")
                           : String8_Lit("[0..]\n");
        return MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    if (value.kind == MemmyEval_ValueKind_RangeList)
    {
        if (result_writer->jsonl)
        {
            for (U64 i = 0; i < value.range_count; i++)
            {
                String8 start = MemmyCli_Address_Format(arena, pointer_width, value.ranges[i].start);
                String8 end = MemmyCli_Address_Format(arena, pointer_width, value.ranges[i].end);
                Memmy_Status status = MemmyCli_EvalResultWriter_Write(
                    result_writer, String8_PushF(arena,
                                                 "{\"type\":\"range\",\"start\":\"%.*s\","
                                                 "\"end\":\"%.*s\"}\n",
                                                 (int)start.len, (char *)start.data, (int)end.len, (char *)end.data));
                if (status != Memmy_Status_Ok)
                {
                    return status;
                }
            }
            return MemmyCli_EvalResultWriter_Write(result_writer,
                                                   String8_PushF(arena, "{\"type\":\"summary\",\"ranges\":%llu}\n",
                                                                 (unsigned long long)value.range_count));
        }

        Memmy_Status status = MemmyCli_EvalResultWriter_Write(result_writer, String8_Lit("START              END\n"));
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        for (U64 i = 0; i < value.range_count; i++)
        {
            String8 start = MemmyCli_Address_Format(arena, pointer_width, value.ranges[i].start);
            String8 end = MemmyCli_Address_Format(arena, pointer_width, value.ranges[i].end);
            status = MemmyCli_EvalResultWriter_Write(result_writer,
                                                     String8_PushF(arena, "%.*s %.*s\n", (int)start.len,
                                                                   (char *)start.data, (int)end.len, (char *)end.data));
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        return Memmy_Status_Ok;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyCli_EvalResultWriter_WriteAddressList(MemmyCli_EvalResultWriter *result_writer,
                                                               MemmyEval_Value value)
{
    Memmy_Status status =
        MemmyCli_ScanOutput_Begin(&result_writer->scan_output, result_writer->arena, result_writer->writer,
                                  MemmyCli_EvalResultWriter_PointerWidth(result_writer), result_writer->jsonl);
    if (status != Memmy_Status_Ok)
    {
        result_writer->status = status;
        return status;
    }
    for (U64 i = 0; i < value.address_count; i++)
    {
        status = MemmyCli_ScanOutput_PushMatch(&result_writer->scan_output, value.addresses[i]);
        if (status != Memmy_Status_Ok)
        {
            result_writer->status = status;
            return status;
        }
    }
    status = MemmyCli_ScanOutput_End(&result_writer->scan_output);
    if (status != Memmy_Status_Ok)
    {
        result_writer->status = status;
    }
    return status;
}

static Memmy_Status MemmyCli_EvalResultWriter_WriteAssignment(MemmyCli_EvalResultWriter *result_writer,
                                                              MemmyEval_Value value)
{
    if (!result_writer->jsonl)
    {
        return Memmy_Status_Ok;
    }

    Arena *arena = result_writer->arena;
    String8 name = MemmyCli_JsonString_Format(arena, result_writer->assignment_name);
    String8 kind = MemmyCli_EvalValue_KindString(value);
    String8 line = String8_PushF(arena, "{\"type\":\"assignment\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
    return MemmyCli_EvalResultWriter_Write(result_writer, line);
}

static Memmy_Status MemmyCli_EvalResultWriter_Push(void *user_data, MemmyEval_Result const *result)
{
    MemmyCli_EvalResultWriter *result_writer = (MemmyCli_EvalResultWriter *)user_data;
    if (result_writer->status != Memmy_Status_Ok)
    {
        return result_writer->status;
    }
    if (result_writer->observer != 0 && result_writer->observer->callback != 0)
    {
        result_writer->status = result_writer->observer->callback(result_writer->observer->user_data, result);
        if (result_writer->status != Memmy_Status_Ok)
        {
            return result_writer->status;
        }
    }

    Arena *arena = result_writer->arena;
    if (result_writer->suppress_value_result &&
        (result->kind == MemmyEval_ResultKind_Value || result->kind == MemmyEval_ResultKind_Read ||
         result->kind == MemmyEval_ResultKind_Write || result->kind == MemmyEval_ResultKind_AddressList))
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteAssignment(result_writer, result->value);
        return result_writer->status;
    }

    MemmyEval_Result value = *result;
    if (value.kind == MemmyEval_ResultKind_Value)
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteValue(result_writer, value.value);
    }
    else if (value.kind == MemmyEval_ResultKind_Read)
    {
        MemmyCli_PeekOutput peek = {
            .pointer_width = MemmyCli_EvalResultWriter_PointerWidth(result_writer),
            .address = value.address,
            .type = value.new_value.type,
            .type_text = MemmyCli_Type_String(value.new_value.type),
            .bytes = value.new_value.bytes,
        };
        String8 output = {0};
        result_writer->status = MemmyCli_PeekOutput_Format(arena, &peek, result_writer->jsonl, &output, 0);
        if (result_writer->status == Memmy_Status_Ok)
        {
            result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, output);
        }
    }
    else if (value.kind == MemmyEval_ResultKind_Write)
    {
        MemmyCli_PokeOutput poke = {
            .pid = MemmyCli_EvalResultWriter_Pid(result_writer),
            .pointer_width = MemmyCli_EvalResultWriter_PointerWidth(result_writer),
            .address = value.address,
            .type = value.new_value.type,
            .type_text = MemmyCli_Type_String(value.new_value.type),
            .old_bytes = value.old_value.bytes,
            .new_bytes = value.new_value.bytes,
            .dry_run = 0,
        };
        String8 output = {0};
        result_writer->status = MemmyCli_PokeOutput_Format(arena, &poke, result_writer->jsonl, &output, 0);
        if (result_writer->status == Memmy_Status_Ok)
        {
            result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, output);
        }
    }
    else if (value.kind == MemmyEval_ResultKind_AddressList)
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteAddressList(result_writer, value.value);
    }
    else if (value.kind == MemmyEval_ResultKind_Process)
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteProcess(result_writer, &value.process);
    }
    else if (value.kind == MemmyEval_ResultKind_Module)
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteModule(result_writer, &value.module);
    }
    else if (value.kind == MemmyEval_ResultKind_Region)
    {
        result_writer->status = MemmyCli_EvalResultWriter_WriteRegion(result_writer, &value.region);
    }
    else if (value.kind == MemmyEval_ResultKind_Variable)
    {
        String8 kind = MemmyCli_EvalValue_KindString(value.variable.value);
        String8 line = {0};
        if (result_writer->jsonl)
        {
            String8 name = MemmyCli_JsonString_Format(arena, value.variable.name);
            line = String8_PushF(arena, "{\"type\":\"variable\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
        }
        else
        {
            line = String8_PushF(arena, "%.*s %.*s\n", (int)value.variable.name.len, (char *)value.variable.name.data,
                                 (int)kind.len, (char *)kind.data);
        }
        result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, line);
    }
    else if (value.kind == MemmyEval_ResultKind_Unset)
    {
        if (result_writer->jsonl)
        {
            String8 name = MemmyCli_JsonString_Format(arena, value.name);
            String8 line =
                String8_PushF(arena, "{\"type\":\"unset\",\"name\":%.*s}\n", (int)name.len, (char *)name.data);
            result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, line);
        }
    }
    else if (value.kind == MemmyEval_ResultKind_Clear)
    {
        if (result_writer->jsonl)
        {
            result_writer->status =
                MemmyCli_EvalResultWriter_Write(result_writer, String8_Lit("{\"type\":\"clear\"}\n"));
        }
    }
    else if (value.kind == MemmyEval_ResultKind_Help)
    {
        String8 help = MemmyCli_Dsl_Help(arena);
        if (result_writer->jsonl)
        {
            String8 text = MemmyCli_JsonString_Format(arena, help);
            String8 line =
                String8_PushF(arena, "{\"type\":\"help\",\"text\":%.*s}\n", (int)text.len, (char *)text.data);
            result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, line);
        }
        else
        {
            result_writer->status = MemmyCli_EvalResultWriter_Write(result_writer, help);
        }
    }
    else if (value.kind == MemmyEval_ResultKind_Exit)
    {
        result_writer->saw_exit = 1;
    }
    return result_writer->status;
}

Memmy_Status MemmyCli_Expr_RunToWriter(Arena *arena, MemmyCli_Options *options, MemmyCli_OutputWriter writer,
                                       Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    return MemmyCli_Expr_RunToWriterWithEnv(arena, env, options, writer, error);
}

Memmy_Status MemmyCli_Expr_RunToWriterWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *options,
                                              MemmyCli_OutputWriter writer, Memmy_Error *error)
{
    if (options == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing options"));
        return Memmy_Status_InvalidArgument;
    }
    return MemmyCli_Statement_RunToWriterWithEnv(arena, env, options, options->expr_text, writer, 0, 0, error);
}

Memmy_Status MemmyCli_Statement_RunToWriterWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *options,
                                                   String8 text, MemmyCli_OutputWriter writer, B32 *out_exit,
                                                   MemmyEval_ResultSink const *observer, Memmy_Error *error)
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

    MemmyAst_Statement statement = {0};
    MemmyAst_Diagnostic diagnostic = {0};
    MemmyAst_Status ast_status = MemmyAst_Statement_Parse(arena, text, &statement, &diagnostic);
    if (ast_status != MemmyAst_Status_Ok)
    {
        Memmy_Status status = MemmyCli_AstStatus_ToMemmyStatus(ast_status);
        MemmyCli_AstError_Set(error, status, &diagnostic);
        return status;
    }
    if (statement.kind == MemmyAst_NodeKind_Command && (statement.command_kind == MemmyAst_CommandKind_Attach ||
                                                        statement.command_kind == MemmyAst_CommandKind_Detach ||
                                                        statement.command_kind == MemmyAst_CommandKind_Tutorial))
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"),
                        String8_Lit("command is only available in a REPL session"));
        return Memmy_Status_InvalidArgument;
    }

    U32 previous_default_pid = 0;
    Memmy_PointerWidth previous_default_pointer_width = Memmy_PointerWidth_Unknown;
    B32 previous_has_default_process =
        MemmyEval_Env_GetDefaultProcess(env, &previous_default_pid, &previous_default_pointer_width);

    Memmy_Status status = MemmyCli_Options_SetProcess(arena, options, env, 0, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    MemmyCli_EvalResultWriter result_writer = {
        .arena = arena,
        .env = env,
        .writer = writer,
        .status = Memmy_Status_Ok,
        .jsonl = options->jsonl,
        .suppress_value_result = statement.kind == MemmyAst_NodeKind_Assignment,
        .assignment_name = statement.assignment_name,
        .observer = observer,
    };
    MemmyEval_ResultSink sink = {
        .callback = MemmyCli_EvalResultWriter_Push,
        .user_data = &result_writer,
    };
    status = MemmyEval_Statement_Eval(arena, env, &statement, &sink, error);
    if (out_exit != 0)
    {
        *out_exit = result_writer.saw_exit;
    }

    if (previous_has_default_process)
    {
        MemmyEval_Env_SetDefaultProcess(env, previous_default_pid, previous_default_pointer_width);
    }
    else
    {
        MemmyEval_Env_ClearDefaultProcess(env);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return result_writer.status;
}
