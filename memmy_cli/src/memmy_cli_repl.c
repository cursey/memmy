#include "memmy_cli_internal.h"

typedef struct Memmy_CliReplStringWriter Memmy_CliReplStringWriter;
struct Memmy_CliReplStringWriter
{
    Arena *arena;
    String8List list;
};

typedef struct Memmy_CliAttachCandidate Memmy_CliAttachCandidate;
struct Memmy_CliAttachCandidate
{
    ListLink link;
    Memmy_ProcessInfo info;
};

typedef struct Memmy_CliAttachResolver Memmy_CliAttachResolver;
struct Memmy_CliAttachResolver
{
    Arena *arena;
    String8 name;
    B32 fuzzy;
    List candidates; // Memmy_CliAttachCandidate
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
    if (session != 0)
    {
        session->has_attached_process = 1;
        session->attached_process = info;
        session->attached_process.name = String8_Copy(session->arena, info.name);
        session->attached_process.path = String8_Copy(session->arena, info.path);
        Memmy_EvalEnv_SetDefaultProcess(session->env, info.pid, info.pointer_width);
        Memmy_EvalEnv_Clear(session->env);
    }
}

static void Memmy_CliReplSession_ClearAttachedProcess(Memmy_CliReplSession *session)
{
    if (session != 0)
    {
        session->has_attached_process = 0;
        session->attached_process = (Memmy_ProcessInfo){0};
        Memmy_EvalEnv_ClearDefaultProcess(session->env);
        Memmy_EvalEnv_Clear(session->env);
    }
}

static Memmy_Status Memmy_CliReplSession_AttachOptions(Arena *arena, Memmy_CliReplSession *session,
                                                       Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options == 0 || session == 0 || (!options->has_pid && !options->has_name))
    {
        return Memmy_Status_Ok;
    }

    Memmy_ProcessInfo info = {0};
    Memmy_Status status = Memmy_Status_Ok;
    if (options->has_pid)
    {
        status = Memmy_Cli_ResolvePidOrOpenTransient(arena, options->pid, &info, error);
    }
    else
    {
        status = Memmy_Cli_ResolveProcessInfo(arena, 0, 0, 1, options->name, &info, error);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_CliReplSession_SetAttachedProcess(session, info);
    return Memmy_Status_Ok;
}

static B32 Memmy_CliRepl_IsDecimalDigits(String8 text)
{
    if (text.len == 0)
    {
        return 0;
    }
    for (U64 i = 0; i < text.len; i++)
    {
        if (!Char8_IsDigit(text.data[i]))
        {
            return 0;
        }
    }
    return 1;
}

static Memmy_Status Memmy_CliAttachResolver_Push(void *user_data, Memmy_ProcessInfo const *info)
{
    Memmy_CliAttachResolver *resolver = (Memmy_CliAttachResolver *)user_data;
    B32 matches = resolver->fuzzy ? String8_FuzzyMatchNoCase(info->name, resolver->name)
                                  : String8_EqNoCase(info->name, resolver->name);
    if (matches)
    {
        Memmy_CliAttachCandidate *candidate = Arena_PushStruct(resolver->arena, Memmy_CliAttachCandidate);
        candidate->info = *info;
        List_PushBack(&resolver->candidates, &candidate->link);
    }
    return Memmy_Status_Ok;
}

static String8 Memmy_CliRepl_FormatAttachCandidates(Arena *arena, List *candidates)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("process name is ambiguous"));
    List_ForEach(Memmy_CliAttachCandidate, candidate, candidates, link)
    {
        String8List_Push(arena, &parts,
                         String8_PushF(arena, "\n%u %.*s", candidate->info.pid, (int)candidate->info.name.len,
                                       (char *)candidate->info.name.data));
    }
    return String8List_Join(arena, &parts, (String8){0});
}

static Memmy_ProcessInfo Memmy_CliRepl_CopyProcessInfo(Arena *arena, Memmy_ProcessInfo info)
{
    info.name = String8_Copy(arena, info.name);
    info.path = String8_Copy(arena, info.path);
    return info;
}

static Memmy_Status Memmy_CliRepl_ResolveAttachArg(Arena *arena, String8 arg, Memmy_ProcessInfo *out,
                                                   Memmy_Error *error)
{
    if (Memmy_CliRepl_IsDecimalDigits(arg))
    {
        Memmy_Size pid64 = 0;
        Memmy_Status status = Memmy_ParseSize(arg, &pid64, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (pid64 > U32_MAX)
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("attach"), String8_Lit("pid overflow"));
            return Memmy_Status_Overflow;
        }
        return Memmy_Cli_ResolvePidOrOpenTransient(arena, (U32)pid64, out, error);
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    Memmy_CliAttachResolver exact = {
        .arena = scratch.arena,
        .name = arg,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = Memmy_CliAttachResolver_Push,
        .user_data = &exact,
    };
    Memmy_Status status = Memmy_EnumerateProcesses(scratch.arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }
    if (exact.candidates.count == 1)
    {
        Memmy_CliAttachCandidate *candidate = ContainerOf(exact.candidates.first, Memmy_CliAttachCandidate, link);
        *out = Memmy_CliRepl_CopyProcessInfo(arena, candidate->info);
        Scratch_End(scratch);
        return Memmy_Status_Ok;
    }
    if (exact.candidates.count > 1)
    {
        String8 message = String8_Copy(arena, Memmy_CliRepl_FormatAttachCandidates(scratch.arena, &exact.candidates));
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("attach"), message);
        return Memmy_Status_Ambiguous;
    }

    Memmy_CliAttachResolver fuzzy = {
        .arena = scratch.arena,
        .name = arg,
        .fuzzy = 1,
    };
    sink.user_data = &fuzzy;
    status = Memmy_EnumerateProcesses(scratch.arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }
    if (fuzzy.candidates.count == 0)
    {
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("attach"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (fuzzy.candidates.count > 1)
    {
        String8 message = String8_Copy(arena, Memmy_CliRepl_FormatAttachCandidates(scratch.arena, &fuzzy.candidates));
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("attach"), message);
        return Memmy_Status_Ambiguous;
    }

    Memmy_CliAttachCandidate *candidate = ContainerOf(fuzzy.candidates.first, Memmy_CliAttachCandidate, link);
    *out = Memmy_CliRepl_CopyProcessInfo(arena, candidate->info);
    Scratch_End(scratch);
    return Memmy_Status_Ok;
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
        .arena = arena,
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

        if (statement.kind == Memmy_AstNodeKind_Command && statement.command_kind == Memmy_AstCommandKind_Attach)
        {
            Memmy_ProcessInfo info = {0};
            Memmy_Status status = Memmy_CliRepl_ResolveAttachArg(arena, statement.command_arg, &info, error);
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
            Memmy_CliReplSession_SetAttachedProcess(session, info);
            if (out != 0)
            {
                *out = (String8){0};
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return Memmy_Status_Ok;
        }
        if (statement.kind == Memmy_AstNodeKind_Command && statement.command_kind == Memmy_AstCommandKind_Detach)
        {
            Memmy_CliReplSession_ClearAttachedProcess(session);
            if (out != 0)
            {
                *out = (String8){0};
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return Memmy_Status_Ok;
        }

        if (session->has_attached_process)
        {
            options.has_name = 0;
            options.name = (String8){0};
            options.has_pid = 1;
            options.pid = session->attached_process.pid;
        }
    }

    Memmy_Status status = Memmy_Cli_RunReplLineWithEnv(arena, session->env, &options, line, out, out_exit, error);
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
    Memmy_Status attach_status = Memmy_CliReplSession_AttachOptions(arena, &session, base_options, error);
    if (attach_status != Memmy_Status_Ok)
    {
        *out = (String8){0};
        return attach_status;
    }
    Memmy_CliOptions line_options = base_options != 0 ? *base_options : (Memmy_CliOptions){0};
    line_options.has_pid = 0;
    line_options.pid = 0;
    line_options.has_name = 0;
    line_options.name = (String8){0};

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
        Memmy_Status status = Memmy_Cli_RunReplSessionLineWithOptions(arena, &session, &line_options, line, &line_out,
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
