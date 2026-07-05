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

Memmy_Status Memmy_Cli_RunReplSessionLine(Arena *arena, Memmy_CliReplSession *session, String8 line, String8 *out,
                                          B32 *out_exit, Memmy_Error *error)
{
    if (session == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing session"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliOptions options = {0};
    return Memmy_Cli_RunReplLineWithEnv(arena, &session->env, &options, line, out, out_exit, error);
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
