#include "memmy_cli_internal.h"

#include "memmy_exec.h"

typedef struct Memmy_CliExprStringWriter Memmy_CliExprStringWriter;
struct Memmy_CliExprStringWriter
{
    Arena *arena;
    String8List list;
};

static Memmy_Status Memmy_CliExprStringWriter_Write(void *user_data, String8 text)
{
    Memmy_CliExprStringWriter *writer = (Memmy_CliExprStringWriter *)user_data;
    String8List_Push(writer->arena, &writer->list, text);
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_VariableKindString(Memmy_VariableExprKind kind)
{
    if (kind == Memmy_VariableExprKind_Address)
    {
        return String8_Lit("address");
    }
    if (kind == Memmy_VariableExprKind_Range)
    {
        return String8_Lit("range");
    }
    if (kind == Memmy_VariableExprKind_Const)
    {
        return String8_Lit("const");
    }
    return String8_Lit("unknown");
}

Memmy_Status Memmy_Cli_RunExpr(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    return Memmy_Cli_RunExprWithEnv(arena, &env, options, out, error);
}

Memmy_Status Memmy_Cli_RunExprWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_CliOptions *options, String8 *out,
                                      Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliExprStringWriter string_writer = {
        .arena = arena,
    };
    Memmy_CliOutputWriter writer = {
        .write = Memmy_CliExprStringWriter_Write,
        .user_data = &string_writer,
    };
    Memmy_Status status = Memmy_Cli_RunExprToWriterWithEnv(arena, env, options, writer, error);
    *out = String8List_Join(arena, &string_writer.list, (String8){0});
    return status;
}

typedef struct Memmy_CliExecResultWriter Memmy_CliExecResultWriter;
struct Memmy_CliExecResultWriter
{
    Arena *arena;
    Memmy_CliOutputWriter writer;
    B32 jsonl;
    B32 process_header_written;
    B32 scan_started;
    B32 saw_exit;
    Memmy_CliScanOutput scan_output;
};

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

static Memmy_Status Memmy_CliExecResultWriter_WriteProcess(Memmy_CliExecResultWriter *result_writer,
                                                           Memmy_ProcessInfo *info)
{
    String8 line = {0};
    String8 arch = Memmy_Cli_PointerWidthArchString(info->pointer_width);
    if (result_writer->jsonl)
    {
        String8 name = Memmy_Cli_FormatJsonString(result_writer->arena, info->name);
        String8 path = Memmy_Cli_FormatJsonString(result_writer->arena, info->path);
        line = String8_PushF(result_writer->arena,
                             "{\"type\":\"process\",\"pid\":%u,\"arch\":\"%.*s\",\"name\":%.*s,\"path\":%.*s}\n",
                             info->pid, (int)arch.len, (char *)arch.data, (int)name.len, (char *)name.data,
                             (int)path.len, (char *)path.data);
    }
    else
    {
        if (!result_writer->process_header_written)
        {
            Memmy_Status status =
                result_writer->writer.write(result_writer->writer.user_data, String8_Lit("PID     ARCH   NAME\n"));
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            result_writer->process_header_written = 1;
        }
        line = String8_PushF(result_writer->arena, "%-7u %-5.*s %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                             (int)info->name.len, (char *)info->name.data);
    }
    return result_writer->writer.write(result_writer->writer.user_data, line);
}

static Memmy_Status Memmy_CliExecResultWriter_Push(void *user_data, Memmy_ExecResult *result)
{
    Memmy_CliExecResultWriter *result_writer = (Memmy_CliExecResultWriter *)user_data;
    Arena *arena = result_writer->arena;
    Memmy_Status status = Memmy_Status_Ok;

    if (result->kind == Memmy_ExecResultKind_Address)
    {
        String8 address_text = Memmy_Cli_FormatAddress(arena, result->address.pointer_width, result->address.address);
        String8 line = {0};
        if (result_writer->jsonl)
        {
            line = String8_PushF(arena, "{\"type\":\"address\",\"address\":\"%.*s\"}\n", (int)address_text.len,
                                 (char *)address_text.data);
        }
        else
        {
            line = String8_PushF(arena, "%.*s\n", (int)address_text.len, (char *)address_text.data);
        }
        status = result_writer->writer.write(result_writer->writer.user_data, line);
    }
    else if (result->kind == Memmy_ExecResultKind_Peek)
    {
        Memmy_CliPeekOutput peek = {
            .pointer_width = result->peek.pointer_width,
            .address = result->peek.address,
            .type = result->peek.type,
            .type_text = Memmy_Cli_TypeString(result->peek.type),
            .bytes = result->peek.value.bytes,
        };
        String8 output = {0};
        status = Memmy_Cli_FormatPeekOutput(arena, &peek, result_writer->jsonl, &output, 0);
        if (status == Memmy_Status_Ok)
        {
            status = result_writer->writer.write(result_writer->writer.user_data, output);
        }
    }
    else if (result->kind == Memmy_ExecResultKind_Poke)
    {
        Memmy_CliPokeOutput poke = {
            .pid = result->poke.pid,
            .pointer_width = result->poke.pointer_width,
            .address = result->poke.address,
            .type = result->poke.type,
            .type_text = Memmy_Cli_TypeString(result->poke.type),
            .old_bytes = result->poke.old_value.bytes,
            .new_bytes = result->poke.new_value.bytes,
            .dry_run = 0,
        };
        String8 output = {0};
        status = Memmy_Cli_FormatPokeOutput(arena, &poke, result_writer->jsonl, &output, 0);
        if (status == Memmy_Status_Ok)
        {
            status = result_writer->writer.write(result_writer->writer.user_data, output);
        }
    }
    else if (result->kind == Memmy_ExecResultKind_Match)
    {
        if (!result_writer->scan_started)
        {
            status = Memmy_CliScanOutput_Begin(&result_writer->scan_output, arena, result_writer->writer,
                                               result->match.pointer_width, result_writer->jsonl);
            result_writer->scan_started = status == Memmy_Status_Ok;
        }
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_CliScanOutput_PushMatch(&result_writer->scan_output, result->match.address);
        }
    }
    else if (result->kind == Memmy_ExecResultKind_Summary)
    {
        if (!result_writer->scan_started)
        {
            status = Memmy_CliScanOutput_Begin(&result_writer->scan_output, arena, result_writer->writer,
                                               Memmy_PointerWidth_64, result_writer->jsonl);
            result_writer->scan_started = status == Memmy_Status_Ok;
        }
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_CliScanOutput_End(&result_writer->scan_output);
        }
    }
    else if (result->kind == Memmy_ExecResultKind_Process)
    {
        status = Memmy_CliExecResultWriter_WriteProcess(result_writer, &result->process.info);
    }
    else if (result->kind == Memmy_ExecResultKind_Assignment)
    {
        String8 kind = Memmy_Cli_VariableKindString(result->assignment.variable_kind);
        String8 line = {0};
        if (result_writer->jsonl)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, result->assignment.name);
            line = String8_PushF(arena, "{\"type\":\"assignment\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
        }
        else
        {
            return Memmy_Status_Ok;
        }
        status = result_writer->writer.write(result_writer->writer.user_data, line);
    }
    else if (result->kind == Memmy_ExecResultKind_VariableBinding)
    {
        String8 kind = Memmy_Cli_VariableKindString(result->variable_binding.variable_kind);
        String8 line = {0};
        if (result_writer->jsonl)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, result->variable_binding.name);
            line = String8_PushF(arena, "{\"type\":\"variable\",\"name\":%.*s,\"kind\":\"%.*s\"}\n", (int)name.len,
                                 (char *)name.data, (int)kind.len, (char *)kind.data);
        }
        else
        {
            line = String8_PushF(arena, "%.*s %.*s\n", (int)result->variable_binding.name.len,
                                 (char *)result->variable_binding.name.data, (int)kind.len, (char *)kind.data);
        }
        status = result_writer->writer.write(result_writer->writer.user_data, line);
    }
    else if (result->kind == Memmy_ExecResultKind_Unset)
    {
        String8 line = {0};
        if (result_writer->jsonl)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, result->unset.name);
            line = String8_PushF(arena, "{\"type\":\"unset\",\"name\":%.*s}\n", (int)name.len, (char *)name.data);
        }
        else
        {
            return Memmy_Status_Ok;
        }
        status = result_writer->writer.write(result_writer->writer.user_data, line);
    }
    else if (result->kind == Memmy_ExecResultKind_Control)
    {
        if (result->control.kind == Memmy_ExecControlKind_Exit)
        {
            result_writer->saw_exit = 1;
        }
    }
    return status;
}

Memmy_Status Memmy_Cli_RunExprToWriter(Arena *arena, Memmy_CliOptions *options, Memmy_CliOutputWriter writer,
                                       Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    return Memmy_Cli_RunExprToWriterWithEnv(arena, &env, options, writer, error);
}

Memmy_Status Memmy_Cli_RunExprToWriterWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_CliOptions *options,
                                              Memmy_CliOutputWriter writer, Memmy_Error *error)
{
    if (arena == 0 || env == 0 || options == 0 || writer.write == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output writer"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Statement statement = {0};
    Memmy_Status status = Memmy_Statement_Parse(arena, options->expr_text, &statement, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_CliExecResultWriter result_writer = {
        .arena = arena,
        .writer = writer,
        .jsonl = options->jsonl,
    };
    Memmy_ExecResultSink sink = {
        .callback = Memmy_CliExecResultWriter_Push,
        .user_data = &result_writer,
    };
    Memmy_ExecProcessSelection selection = {
        .has_pid = options->has_pid,
        .pid = options->pid,
        .has_name = options->has_name,
        .name = options->name,
    };
    return Memmy_Statement_ExecuteWithEnv(arena, env, &statement, selection, sink, error);
}

Memmy_Status Memmy_Cli_RunStatementToWriterWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_CliOptions *options,
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

    Memmy_Statement statement = {0};
    Memmy_Status status = Memmy_Statement_Parse(arena, text, &statement, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_CliExecResultWriter result_writer = {
        .arena = arena,
        .writer = writer,
        .jsonl = options->jsonl,
    };
    Memmy_ExecResultSink sink = {
        .callback = Memmy_CliExecResultWriter_Push,
        .user_data = &result_writer,
    };
    Memmy_ExecProcessSelection selection = {
        .has_pid = options->has_pid,
        .pid = options->pid,
        .has_name = options->has_name,
        .name = options->name,
    };
    status = Memmy_Statement_ExecuteWithEnv(arena, env, &statement, selection, sink, error);
    if (out_exit != 0)
    {
        *out_exit = result_writer.saw_exit;
    }
    return status;
}
