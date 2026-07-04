#include "memmy_cli_internal.h"

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
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("repl"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    String8 expr = String8_TrimWhitespace(line);
    if (expr.len == 0 || String8_Eq(expr, String8_Lit("quit")) || String8_Eq(expr, String8_Lit("exit")))
    {
        return Memmy_Status_Ok;
    }

    Memmy_CliOptions options = base_options != 0 ? *base_options : (Memmy_CliOptions){0};
    options.has_expr = 1;
    options.expr_text = expr;
    return Memmy_Cli_RunExpr(arena, &options, out, error);
}

Memmy_Status Memmy_Cli_RunReplLine(Arena *arena, String8 line, String8 *out, Memmy_Error *error)
{
    Memmy_CliOptions options = {0};
    return Memmy_Cli_RunReplLineWithOptions(arena, &options, line, out, error);
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

        String8 trimmed = String8_TrimWhitespace(line);
        if (String8_Eq(trimmed, String8_Lit("quit")) || String8_Eq(trimmed, String8_Lit("exit")))
        {
            break;
        }

        Memmy_Error line_error = {0};
        String8 line_out = {0};
        Memmy_Status status = Memmy_Cli_RunReplLineWithOptions(arena, base_options, line, &line_out, &line_error);
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
    }

    *out = String8List_Join(arena, &parts, (String8){0});
    return result;
}
