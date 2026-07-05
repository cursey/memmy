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

static Memmy_Status Memmy_Cli_RunReplLineWithEnv(Arena *arena, Memmy_EvalEnv *env, Memmy_CliOptions *base_options,
                                                 String8 line, String8 *out, B32 *out_exit, Memmy_Error *error);
static Memmy_Status Memmy_Cli_RunReplSessionLineWithOptions(Arena *arena, Memmy_CliReplSession *session,
                                                            Memmy_CliOptions *base_options, String8 line, String8 *out,
                                                            B32 *out_exit, Memmy_Error *error);

static void Memmy_CliReplSession_SetAttachedProcess(Memmy_CliReplSession *session, Memmy_ProcessInfo info)
{
    session->has_attached_process = 1;
    session->attached_process = info;
    session->attached_process.name = String8_Copy(session->env->arena, info.name);
    session->attached_process.path = String8_Copy(session->env->arena, info.path);
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
    if (Memmy_Cli_ResolveProcessInfo(arena, 1, session->attached_process.pid, 0, (String8){0}, &info, 0) ==
        Memmy_Status_Ok)
    {
        Memmy_CliReplSession_SetAttachedProcess(session, info);
    }
}

static Memmy_Status Memmy_CliRepl_AttachCandidate(Arena *arena, Memmy_CliTargetProcess target, Memmy_ProcessInfo *out,
                                                  Memmy_Error *error)
{
    if (!target.found || out == 0)
    {
        return Memmy_Status_NotFound;
    }
    if (target.is_pid)
    {
        out->pid = target.pid;
        return Memmy_Status_Ok;
    }
    return Memmy_Cli_ResolveProcessInfo(arena, 0, 0, 1, target.name, out, error);
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

static Memmy_Status Memmy_Cli_RunReplLineWithEnv(Arena *arena, Memmy_EvalEnv *env, Memmy_CliOptions *base_options,
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
        .env = Memmy_EvalEnv_Create(arena),
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
    return Memmy_Cli_RunReplSessionLineWithOptions(arena, session, 0, line, out, out_exit, error);
}

static Memmy_Status Memmy_Cli_RunReplSessionLineWithOptions(Arena *arena, Memmy_CliReplSession *session,
                                                            Memmy_CliOptions *base_options, String8 line, String8 *out,
                                                            B32 *out_exit, Memmy_Error *error)
{
    if (session == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing session"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliOptions options = base_options != 0 ? *base_options : (Memmy_CliOptions){0};
    B32 has_base_process_selector = options.has_pid || options.has_name;
    Memmy_CliTargetProcess target = {0};
    Memmy_ProcessInfo attach_candidate = {0};
    B32 has_attach_candidate = 0;

    String8 statement_text = String8_TrimWhitespace(line);
    if (statement_text.len != 0)
    {
        Memmy_AstStatement statement = {0};
        Memmy_AstDiagnostic diagnostic = {0};
        Memmy_AstStatus ast_status = Memmy_Ast_ParseStatement(arena, statement_text, &statement, &diagnostic);
        if (ast_status != Memmy_AstStatus_Ok)
        {
            if (out != 0)
            {
                *out = (String8){0};
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            Memmy_Status status = ast_status == Memmy_AstStatus_Overflow          ? Memmy_Status_Overflow
                                  : ast_status == Memmy_AstStatus_Unsupported     ? Memmy_Status_Unsupported
                                  : ast_status == Memmy_AstStatus_InvalidArgument ? Memmy_Status_InvalidArgument
                                                                                  : Memmy_Status_ParseError;
            Memmy_Error_Set(error, status, String8_Lit("expr"), diagnostic.message);
            if (error != 0)
            {
                error->input = diagnostic.input;
                error->byte_offset = diagnostic.byte_offset;
                error->byte_count = diagnostic.byte_count;
            }
            return status;
        }

        target = Memmy_Cli_StatementTargetProcess(&statement);

        if (!target.found && session->has_attached_process && !has_base_process_selector)
        {
            options.has_name = 0;
            options.name = (String8){0};
            options.has_pid = 1;
            options.pid = session->attached_process.pid;
        }
        else if (target.found)
        {
            Memmy_Status status = Memmy_CliRepl_AttachCandidate(arena, target, &attach_candidate, error);
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

    Memmy_Status status = Memmy_Cli_RunReplLineWithEnv(arena, session->env, &options, line, out, out_exit, error);
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
    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
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
        Memmy_Status status = Memmy_Cli_RunReplSessionLineWithOptions(arena, &session, base_options, line, &line_out,
                                                                      &should_exit, &line_error);
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
