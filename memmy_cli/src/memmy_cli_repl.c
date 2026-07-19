#include "memmy_cli_internal.h"

typedef struct MemmyCli_ReplStringWriter MemmyCli_ReplStringWriter;
struct MemmyCli_ReplStringWriter
{
    Arena *arena;
    String8List list;
};

typedef struct MemmyCli_AttachCandidate MemmyCli_AttachCandidate;
struct MemmyCli_AttachCandidate
{
    ListLink link;
    Memmy_ProcessInfo info;
};

typedef struct MemmyCli_AttachResolver MemmyCli_AttachResolver;
struct MemmyCli_AttachResolver
{
    Arena *arena;
    String8 name;
    B32 fuzzy;
    List candidates; // MemmyCli_AttachCandidate
};

static Memmy_Status MemmyCli_ReplStringWriter_Write(void *user_data, String8 text)
{
    MemmyCli_ReplStringWriter *writer = (MemmyCli_ReplStringWriter *)user_data;
    String8List_Push(writer->arena, &writer->list, text);
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyCli_Repl_RunLineWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *base_options,
                                                 String8 line, String8 *out, B32 *out_exit,
                                                 MemmyEval_ResultSink const *observer, Memmy_Error *error);
static Memmy_Status MemmyCli_ReplSession_RunLineWithOptions(Arena *arena, MemmyCli_ReplSession *session,
                                                            MemmyCli_Options *base_options, String8 line, String8 *out,
                                                            B32 *out_exit, Memmy_Error *error);

static void MemmyCli_ReplSession_SetAttachedProcess(MemmyCli_ReplSession *session, Memmy_ProcessInfo info)
{
    if (session != 0)
    {
        session->has_attached_process = 1;
        session->attached_process = info;
        session->attached_process.name = String8_Copy(session->arena, info.name);
        session->attached_process.path = String8_Copy(session->arena, info.path);
        MemmyEval_Env_SetDefaultProcess(session->env, info.pid, info.pointer_width);
        MemmyEval_Env_Clear(session->env);
    }
}

static void MemmyCli_ReplSession_ClearAttachedProcess(MemmyCli_ReplSession *session)
{
    if (session != 0)
    {
        session->has_attached_process = 0;
        session->attached_process = (Memmy_ProcessInfo){0};
        MemmyEval_Env_ClearDefaultProcess(session->env);
        MemmyEval_Env_Clear(session->env);
    }
}

static Memmy_Status MemmyCli_ReplSession_AttachOptions(Arena *arena, MemmyCli_ReplSession *session,
                                                       MemmyCli_Options *options, Memmy_Error *error)
{
    if (options == 0 || session == 0 || (!options->has_pid && !options->has_name))
    {
        return Memmy_Status_Ok;
    }

    Memmy_ProcessInfo info = {0};
    Memmy_Status status = Memmy_Status_Ok;
    if (options->has_pid)
    {
        status = MemmyCli_Pid_ResolveOrOpenTransient(arena, options->pid, &info, error);
    }
    else
    {
        status = MemmyCli_ProcessInfo_Resolve(arena, 0, 0, 1, options->name, &info, error);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    MemmyCli_ReplSession_SetAttachedProcess(session, info);
    return Memmy_Status_Ok;
}

static B32 MemmyCli_Repl_IsDecimalDigits(String8 text)
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

static Memmy_Status MemmyCli_AttachResolver_Push(void *user_data, Memmy_ProcessInfo const *info)
{
    MemmyCli_AttachResolver *resolver = (MemmyCli_AttachResolver *)user_data;
    B32 matches = resolver->fuzzy ? String8_FuzzyMatchNoCase(info->name, resolver->name)
                                  : String8_EqNoCase(info->name, resolver->name);
    if (matches)
    {
        MemmyCli_AttachCandidate *candidate = Arena_PushStruct(resolver->arena, MemmyCli_AttachCandidate);
        candidate->info = *info;
        List_PushBack(&resolver->candidates, &candidate->link);
    }
    return Memmy_Status_Ok;
}

static String8 MemmyCli_Repl_FormatAttachCandidates(Arena *arena, List *candidates)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("process name is ambiguous"));
    List_ForEach(MemmyCli_AttachCandidate, candidate, candidates, link)
    {
        String8List_Push(arena, &parts,
                         String8_PushF(arena, "\n%u %.*s", candidate->info.pid, (int)candidate->info.name.len,
                                       (char *)candidate->info.name.data));
    }
    return String8List_Join(arena, &parts, (String8){0});
}

static Memmy_ProcessInfo MemmyCli_Repl_CopyProcessInfo(Arena *arena, Memmy_ProcessInfo info)
{
    info.name = String8_Copy(arena, info.name);
    info.path = String8_Copy(arena, info.path);
    return info;
}

static Memmy_Status MemmyCli_Repl_ResolveAttachArg(Arena *arena, String8 arg, Memmy_ProcessInfo *out,
                                                   Memmy_Error *error)
{
    if (MemmyCli_Repl_IsDecimalDigits(arg))
    {
        Memmy_Size pid64 = 0;
        Memmy_Status status = Memmy_Size_Parse(arg, &pid64, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (pid64 > U32_MAX)
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("attach"), String8_Lit("pid overflow"));
            return Memmy_Status_Overflow;
        }
        return MemmyCli_Pid_ResolveOrOpenTransient(arena, (U32)pid64, out, error);
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    MemmyCli_AttachResolver exact = {
        .arena = scratch.arena,
        .name = arg,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = MemmyCli_AttachResolver_Push,
        .user_data = &exact,
    };
    Memmy_Status status = Memmy_Process_Enumerate(scratch.arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }
    if (exact.candidates.count == 1)
    {
        MemmyCli_AttachCandidate *candidate = ContainerOf(exact.candidates.first, MemmyCli_AttachCandidate, link);
        *out = MemmyCli_Repl_CopyProcessInfo(arena, candidate->info);
        Scratch_End(scratch);
        return Memmy_Status_Ok;
    }
    if (exact.candidates.count > 1)
    {
        String8 message = String8_Copy(arena, MemmyCli_Repl_FormatAttachCandidates(scratch.arena, &exact.candidates));
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("attach"), message);
        return Memmy_Status_Ambiguous;
    }

    MemmyCli_AttachResolver fuzzy = {
        .arena = scratch.arena,
        .name = arg,
        .fuzzy = 1,
    };
    sink.user_data = &fuzzy;
    status = Memmy_Process_Enumerate(scratch.arena, sink, error);
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
        String8 message = String8_Copy(arena, MemmyCli_Repl_FormatAttachCandidates(scratch.arena, &fuzzy.candidates));
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("attach"), message);
        return Memmy_Status_Ambiguous;
    }

    MemmyCli_AttachCandidate *candidate = ContainerOf(fuzzy.candidates.first, MemmyCli_AttachCandidate, link);
    *out = MemmyCli_Repl_CopyProcessInfo(arena, candidate->info);
    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

static String8 MemmyCli_TextError_Format(Arena *arena, Memmy_Status status, Memmy_Error *error)
{
    if (error != 0 && error->message.len > 0)
    {
        return String8_PushF(arena, "memmy: %s: %.*s\n", Memmy_Status_Name(status), (int)error->message.len,
                             (char *)error->message.data);
    }
    return String8_PushF(arena, "memmy: %s\n", Memmy_Status_Name(status));
}

static String8 MemmyCli_Repl_AppendTutorialText(Arena *arena, String8 ordinary, String8 tutorial)
{
    if (tutorial.len == 0)
    {
        return ordinary;
    }
    if (ordinary.len == 0)
    {
        return tutorial;
    }

    String8 separator = ordinary.data[ordinary.len - 1] == '\n' ? String8_Lit("\n") : String8_Lit("\n\n");
    String8List parts = {0};
    String8List_Push(arena, &parts, ordinary);
    String8List_Push(arena, &parts, separator);
    String8List_Push(arena, &parts, tutorial);
    return String8List_Join(arena, &parts, (String8){0});
}

static Memmy_Status MemmyCli_Repl_RunLineWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *base_options,
                                                 String8 line, String8 *out, B32 *out_exit,
                                                 MemmyEval_ResultSink const *observer, Memmy_Error *error)
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

    MemmyCli_Options options = base_options != 0 ? *base_options : (MemmyCli_Options){0};
    MemmyCli_ReplStringWriter string_writer = {
        .arena = arena,
    };
    MemmyCli_OutputWriter writer = {
        .write = MemmyCli_ReplStringWriter_Write,
        .user_data = &string_writer,
    };
    Memmy_Status status =
        MemmyCli_Statement_RunToWriterWithEnv(arena, env, &options, statement, writer, out_exit, observer, error);
    *out = String8List_Join(arena, &string_writer.list, (String8){0});
    return status;
}

Memmy_Status MemmyCli_Repl_RunLine(Arena *arena, String8 line, String8 *out, Memmy_Error *error)
{
    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    return MemmyCli_ReplSession_RunLine(arena, &session, line, out, 0, error);
}

MemmyCli_ReplSession MemmyCli_ReplSession_Begin(Arena *arena)
{
    return (MemmyCli_ReplSession){
        .arena = arena,
        .env = MemmyEval_Env_Create(arena),
    };
}

String8 MemmyCli_ReplSession_Prompt(Arena *arena, MemmyCli_ReplSession *session)
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

Memmy_Status MemmyCli_ReplSession_RunLine(Arena *arena, MemmyCli_ReplSession *session, String8 line, String8 *out,
                                          B32 *out_exit, Memmy_Error *error)
{
    return MemmyCli_ReplSession_RunLineWithOptions(arena, session, 0, line, out, out_exit, error);
}

static Memmy_Status MemmyCli_ReplSession_RunLineWithOptions(Arena *arena, MemmyCli_ReplSession *session,
                                                            MemmyCli_Options *base_options, String8 line, String8 *out,
                                                            B32 *out_exit, Memmy_Error *error)
{
    if (session == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing session"));
        return Memmy_Status_InvalidArgument;
    }

    MemmyCli_Options options = base_options != 0 ? *base_options : (MemmyCli_Options){0};

    String8 statement_text = String8_TrimWhitespace(line);
    MemmyAst_Statement statement = {0};
    if (statement_text.len != 0)
    {
        MemmyAst_Diagnostic diagnostic = {0};
        MemmyAst_Status ast_status = MemmyAst_Statement_Parse(arena, statement_text, &statement, &diagnostic);
        if (ast_status != MemmyAst_Status_Ok)
        {
            if (out != 0)
            {
                *out = (String8){0};
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            Memmy_Status status = ast_status == MemmyAst_Status_Overflow          ? Memmy_Status_Overflow
                                  : ast_status == MemmyAst_Status_Unsupported     ? Memmy_Status_Unsupported
                                  : ast_status == MemmyAst_Status_InvalidArgument ? Memmy_Status_InvalidArgument
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

        if (statement.kind == MemmyAst_NodeKind_Command && statement.command_kind == MemmyAst_CommandKind_Attach)
        {
            Memmy_ProcessInfo info = {0};
            Memmy_Status status = MemmyCli_Repl_ResolveAttachArg(arena, statement.command_arg, &info, error);
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
            MemmyCli_ReplSession_SetAttachedProcess(session, info);
            String8 tutorial_out = MemmyCli_Tutorial_Statement_End(arena, session->tutorial, &statement,
                                                                   Memmy_Status_Ok, session->has_attached_process,
                                                                   session->attached_process.pid, session->env);
            if (out != 0)
            {
                *out = tutorial_out;
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return Memmy_Status_Ok;
        }
        if (statement.kind == MemmyAst_NodeKind_Command && statement.command_kind == MemmyAst_CommandKind_Detach)
        {
            MemmyCli_ReplSession_ClearAttachedProcess(session);
            String8 tutorial_out = MemmyCli_Tutorial_Statement_End(
                arena, session->tutorial, &statement, Memmy_Status_Ok, session->has_attached_process, 0, session->env);
            if (out != 0)
            {
                *out = tutorial_out;
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return Memmy_Status_Ok;
        }
        if (statement.kind == MemmyAst_NodeKind_Command && statement.command_kind == MemmyAst_CommandKind_Tutorial)
        {
            if (options.jsonl)
            {
                if (out != 0)
                {
                    *out = (String8){0};
                }
                if (out_exit != 0)
                {
                    *out_exit = 0;
                }
                Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("tutorial"),
                                String8_Lit("tutorial is not available with --jsonl"));
                if (error != 0)
                {
                    error->input = statement.text;
                }
                return Memmy_Status_InvalidArgument;
            }
            if (session->tutorial == 0)
            {
                session->tutorial = MemmyCli_Tutorial_Create(session->arena);
            }
            if (out_exit != 0)
            {
                *out_exit = 0;
            }
            return MemmyCli_Tutorial_Command_Run(arena, session->tutorial, statement.command_arg, out, error);
        }

        if (session->has_attached_process)
        {
            options.has_name = 0;
            options.name = (String8){0};
            options.has_pid = 1;
            options.pid = session->attached_process.pid;
        }
    }

    MemmyEval_ResultSink const *observer = MemmyCli_Tutorial_Statement_Begin(session->tutorial);
    Memmy_Status status =
        MemmyCli_Repl_RunLineWithEnv(arena, session->env, &options, line, out, out_exit, observer, error);
    if (statement_text.len != 0)
    {
        String8 tutorial_out = MemmyCli_Tutorial_Statement_End(
            arena, session->tutorial, &statement, status, session->has_attached_process,
            session->has_attached_process ? session->attached_process.pid : 0, session->env);
        if (out != 0)
        {
            *out = MemmyCli_Repl_AppendTutorialText(arena, *out, tutorial_out);
        }
    }
    return status;
}

Memmy_Status MemmyCli_Repl_RunString(Arena *arena, String8 input, String8 *out, Memmy_Error *error)
{
    MemmyCli_Options options = {0};
    return MemmyCli_Repl_RunStringWithOptions(arena, &options, input, out, error);
}

Memmy_Status MemmyCli_Repl_RunStringWithOptions(Arena *arena, MemmyCli_Options *base_options, String8 input,
                                                String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    String8List parts = {0};
    Memmy_Status result = Memmy_Status_Ok;
    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    Memmy_Status attach_status = MemmyCli_ReplSession_AttachOptions(arena, &session, base_options, error);
    if (attach_status != Memmy_Status_Ok)
    {
        *out = (String8){0};
        return attach_status;
    }
    MemmyCli_Options line_options = base_options != 0 ? *base_options : (MemmyCli_Options){0};
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
        Memmy_Status status = MemmyCli_ReplSession_RunLineWithOptions(arena, &session, &line_options, line, &line_out,
                                                                      &should_exit, &line_error);
        if (status == Memmy_Status_Ok)
        {
            String8List_Push(arena, &parts, line_out);
        }
        else
        {
            if (line_error.status == Memmy_Status_Ok)
            {
                line_error.status = status;
            }
            if (result == Memmy_Status_Ok)
            {
                result = status;
                if (error != 0)
                {
                    *error = line_error;
                }
            }
            String8 formatted_error = line_options.jsonl ? MemmyCli_JsonlError_Format(arena, &line_error)
                                                         : MemmyCli_TextError_Format(arena, status, &line_error);
            String8List_Push(arena, &parts, formatted_error);
        }
        if (should_exit)
        {
            break;
        }
    }

    *out = String8List_Join(arena, &parts, (String8){0});
    return result;
}
