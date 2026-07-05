#include "memmy_cli_internal.h"

typedef struct Memmy_CliReplStringWriter Memmy_CliReplStringWriter;
struct Memmy_CliReplStringWriter
{
    Arena *arena;
    String8List list;
};

static Memmy_Status Memmy_CliReplStringWriter_Write(void *user_data, String8 text)
{
    Memmy_CliReplStringWriter *writer = (Memmy_CliReplStringWriter *)user_data;
    String8List_Push(writer->arena, &writer->list, text);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RunReplLineWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_CliOptions *base_options,
                                                 String8 line, String8 *out, B32 *out_exit, Memmy_Error *error);
static Memmy_Status Memmy_Cli_ResolveProcessInfo(Arena *arena, Memmy_ProcessSelector selector, Memmy_ProcessInfo *out,
                                                 Memmy_Error *error);

typedef struct Memmy_CliProcessInfoResolver Memmy_CliProcessInfoResolver;
struct Memmy_CliProcessInfoResolver
{
    Memmy_ProcessSelector selector;
    Memmy_ProcessInfo match;
    U64 match_count;
};

static Memmy_Status Memmy_CliProcessInfoResolver_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_CliProcessInfoResolver *resolver = (Memmy_CliProcessInfoResolver *)user_data;
    B32 matches = 0;
    if (resolver->selector.kind == Memmy_ProcessSelectorKind_Pid)
    {
        matches = info->pid == resolver->selector.pid;
    }
    else if (resolver->selector.kind == Memmy_ProcessSelectorKind_Name)
    {
        matches = String8_EqNoCase(info->name, resolver->selector.name);
    }

    if (matches)
    {
        resolver->match = *info;
        resolver->match_count++;
    }
    return Memmy_Status_Ok;
}

static void Memmy_CliReplSession_SetAttachedProcess(Memmy_CliReplSession *session, Memmy_ProcessInfo info)
{
    session->has_attached_process = 1;
    session->attached_process = info;
    session->attached_process.name = String8_Copy(session->env.arena, info.name);
    session->attached_process.path = String8_Copy(session->env.arena, info.path);
}

static void Memmy_CliReplSession_SetAttachedPid(Memmy_CliReplSession *session, U32 pid)
{
    session->has_attached_process = 1;
    session->attached_process = (Memmy_ProcessInfo){
        .pid = pid,
    };
}

static void Memmy_CliReplSession_TryFillAttachedName(Arena *arena, Memmy_CliReplSession *session)
{
    if (session == 0 || !session->has_attached_process || session->attached_process.name.len != 0)
    {
        return;
    }

    Memmy_ProcessInfo info = {0};
    Memmy_ProcessSelector selector = {
        .kind = Memmy_ProcessSelectorKind_Pid,
        .pid = session->attached_process.pid,
    };
    if (Memmy_Cli_ResolveProcessInfo(arena, selector, &info, 0) == Memmy_Status_Ok)
    {
        Memmy_CliReplSession_SetAttachedProcess(session, info);
    }
}

static Memmy_Status Memmy_Cli_ResolveProcessInfo(Arena *arena, Memmy_ProcessSelector selector, Memmy_ProcessInfo *out,
                                                 Memmy_Error *error)
{
    if (selector.kind == Memmy_ProcessSelectorKind_None || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                        String8_Lit("missing process selector"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliProcessInfoResolver resolver = {
        .selector = selector,
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

static Memmy_ProcessSelector Memmy_CliRepl_AddressProcessSelector(Memmy_ExecEnv *env, Memmy_AddressExpr *address);
static Memmy_ProcessSelector Memmy_CliRepl_RangeProcessSelector(Memmy_ExecEnv *env, Memmy_RangeExpr *range);

static Memmy_ProcessSelector Memmy_CliRepl_AddressProcessSelector(Memmy_ExecEnv *env, Memmy_AddressExpr *address)
{
    Memmy_ProcessSelector selector = {0};
    if (address->base_kind == Memmy_AddressExprBaseKind_Target ||
        address->base_kind == Memmy_AddressExprBaseKind_ProcessAbsolute)
    {
        selector = address->target.process;
    }
    else if (address->base_kind == Memmy_AddressExprBaseKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, address->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Address)
        {
            if (Memmy_ExecEnv_ResolvePush(env, address->variable.name, 0) == Memmy_Status_Ok)
            {
                selector = Memmy_CliRepl_AddressProcessSelector(env, &Memmy_ExecVariableBinding_Expr(binding)->address);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }
    return selector;
}

static Memmy_ProcessSelector Memmy_CliRepl_RangeProcessSelector(Memmy_ExecEnv *env, Memmy_RangeExpr *range)
{
    Memmy_ProcessSelector selector = {0};
    if (range->kind == Memmy_RangeExprKind_Target || range->kind == Memmy_RangeExprKind_TargetOffset ||
        range->kind == Memmy_RangeExprKind_TargetSized)
    {
        selector = range->target.process;
    }
    else if (range->kind == Memmy_RangeExprKind_AddressSized)
    {
        selector = Memmy_CliRepl_AddressProcessSelector(env, &range->address);
    }
    else if (range->kind == Memmy_RangeExprKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, range->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Range)
        {
            if (Memmy_ExecEnv_ResolvePush(env, range->variable.name, 0) == Memmy_Status_Ok)
            {
                selector = Memmy_CliRepl_RangeProcessSelector(env, &Memmy_ExecVariableBinding_Expr(binding)->range);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }
    return selector;
}

static Memmy_ProcessSelector Memmy_CliRepl_MemoryProcessSelector(Memmy_ExecEnv *env, Memmy_MemoryExpr *expr)
{
    if (expr->kind == Memmy_MemoryExprKind_Address || expr->kind == Memmy_MemoryExprKind_Peek ||
        expr->kind == Memmy_MemoryExprKind_Poke)
    {
        return Memmy_CliRepl_AddressProcessSelector(env, &expr->address);
    }
    if (expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        return Memmy_CliRepl_RangeProcessSelector(env, &expr->range);
    }
    return (Memmy_ProcessSelector){0};
}

static String8 Memmy_Cli_FormatTextError(Arena *arena, Memmy_Status status, Memmy_Error *error)
{
    if (error != 0 && error->message.len > 0)
    {
        return String8_PushF(arena, "memmy: %s: %.*s\n", Memmy_Status_Name(status), (int)error->message.len,
                             (char *)error->message.data);
    }
    return String8_PushF(arena, "memmy: %s\n", Memmy_Status_Name(status));
}

static Memmy_Status Memmy_Cli_RunReplLineWithOptions(Arena *arena, Memmy_CliOptions *base_options, String8 line,
                                                     String8 *out, Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    return Memmy_Cli_RunReplLineWithEnv(arena, &env, base_options, line, out, 0, error);
}

static Memmy_Status Memmy_Cli_RunReplLineWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_CliOptions *base_options,
                                                 String8 line, String8 *out, B32 *out_exit, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    if (out_exit != 0)
    {
        *out_exit = 0;
    }
    String8 statement = String8_TrimWhitespace(line);
    if (statement.len == 0)
    {
        return Memmy_Status_Ok;
    }

    Memmy_CliOptions options = base_options != 0 ? *base_options : (Memmy_CliOptions){0};
    Memmy_CliReplStringWriter string_writer = {
        .arena = arena,
    };
    Memmy_CliOutputWriter writer = {
        .write = Memmy_CliReplStringWriter_Write,
        .user_data = &string_writer,
    };
    Memmy_Status status =
        Memmy_Cli_RunStatementToWriterWithEnv(arena, env, &options, statement, writer, out_exit, error);
    *out = String8List_Join(arena, &string_writer.list, (String8){0});
    return status;
}

Memmy_Status Memmy_Cli_RunReplLine(Arena *arena, String8 line, String8 *out, Memmy_Error *error)
{
    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    return Memmy_Cli_RunReplSessionLine(arena, &session, line, out, 0, error);
}

Memmy_CliReplSession Memmy_CliReplSession_Begin(Arena *arena)
{
    return (Memmy_CliReplSession){
        .env = Memmy_ExecEnv_Create(arena),
    };
}

String8 Memmy_CliReplSession_Prompt(Arena *arena, Memmy_CliReplSession *session)
{
    if (session != 0 && session->has_attached_process)
    {
        if (session->attached_process.name.len != 0)
        {
            return String8_PushF(arena, "[%.*s:%u]> ", (int)session->attached_process.name.len,
                                 (char *)session->attached_process.name.data, session->attached_process.pid);
        }
        return String8_PushF(arena, "[%u]> ", session->attached_process.pid);
    }
    return String8_Lit("> ");
}

Memmy_Status Memmy_Cli_RunReplSessionLine(Arena *arena, Memmy_CliReplSession *session, String8 line, String8 *out,
                                          B32 *out_exit, Memmy_Error *error)
{
    if (session == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing session"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliOptions options = {0};
    Memmy_ProcessSelector selector = {0};
    Memmy_ProcessInfo attach_candidate = {0};
    B32 has_attach_candidate = 0;

    String8 statement_text = String8_TrimWhitespace(line);
    if (statement_text.len != 0)
    {
        Memmy_Statement statement = {0};
        Memmy_Status status = Memmy_Statement_Parse(arena, statement_text, &statement, error);
        if (status != Memmy_Status_Ok)
        {
            if (out != 0)
            {
                *out = (String8){0};
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return status;
        }

        if (statement.kind == Memmy_StatementKind_Memory)
        {
            selector = Memmy_CliRepl_MemoryProcessSelector(&session->env, &statement.memory);
        }

        if (selector.kind == Memmy_ProcessSelectorKind_None && session->has_attached_process)
        {
            options.has_pid = 1;
            options.pid = session->attached_process.pid;
        }
        else if (selector.kind == Memmy_ProcessSelectorKind_Pid && !session->has_attached_process)
        {
            attach_candidate.pid = selector.pid;
            has_attach_candidate = 1;
        }
        else if (selector.kind == Memmy_ProcessSelectorKind_Name && !session->has_attached_process)
        {
            status = Memmy_Cli_ResolveProcessInfo(arena, selector, &attach_candidate, error);
            if (status != Memmy_Status_Ok)
            {
                if (out != 0)
                {
                    *out = (String8){0};
                }
                if (out_exit != 0)
                {
                    *out_exit = 0;
                }
                return status;
            }
            has_attach_candidate = 1;
        }
    }

    Memmy_Status status = Memmy_Cli_RunReplLineWithEnv(arena, &session->env, &options, line, out, out_exit, error);
    if (status == Memmy_Status_Ok && has_attach_candidate)
    {
        if (attach_candidate.name.len != 0)
        {
            Memmy_CliReplSession_SetAttachedProcess(session, attach_candidate);
        }
        else
        {
            Memmy_CliReplSession_SetAttachedPid(session, attach_candidate.pid);
            Memmy_CliReplSession_TryFillAttachedName(arena, session);
        }
    }
    return status;
}

Memmy_Status Memmy_Cli_RunReplString(Arena *arena, String8 input, String8 *out, Memmy_Error *error)
{
    Memmy_CliOptions options = {0};
    return Memmy_Cli_RunReplStringWithOptions(arena, &options, input, out, error);
}

Memmy_Status Memmy_Cli_RunReplStringWithOptions(Arena *arena, Memmy_CliOptions *base_options, String8 input,
                                                String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    String8List parts = {0};
    Memmy_Status result = Memmy_Status_Ok;
    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    U64 line_start = 0;
    for (U64 i = 0; i <= input.len; i++)
    {
        if (i != input.len && input.data[i] != '\n')
        {
            continue;
        }

        U64 line_len = i - line_start;
        if (line_len > 0 && input.data[line_start + line_len - 1] == '\r')
        {
            line_len--;
        }
        String8 line = String8_Substr(input, line_start, line_len);
        line_start = i + 1;

        Memmy_Error line_error = {0};
        String8 line_out = {0};
        B32 should_exit = 0;
        Memmy_Status status =
            Memmy_Cli_RunReplLineWithEnv(arena, &env, base_options, line, &line_out, &should_exit, &line_error);
        if (status == Memmy_Status_Ok)
        {
            String8List_Push(arena, &parts, line_out);
        }
        else
        {
            if (result == Memmy_Status_Ok)
            {
                result = status;
                if (error != 0)
                {
                    *error = line_error;
                }
            }
            String8List_Push(arena, &parts, Memmy_Cli_FormatTextError(arena, status, &line_error));
        }
        if (should_exit)
        {
            break;
        }
    }

    *out = String8List_Join(arena, &parts, (String8){0});
    return result;
}
