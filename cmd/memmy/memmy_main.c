#include <stdio.h>

#include "memmy.h"
#include "memmy_cli.h"

static Memmy_Status Memmy_Main_RunRepl(Arena *arena)
{
    char line[4096];
    Memmy_Status result = Memmy_Status_Ok;
    while (fgets(line, sizeof(line), stdin) != 0)
    {
        Scratch scratch = Scratch_Begin(&arena, 1);
        Memmy_Error error = {0};
        String8 output = {0};
        Memmy_Status status = Memmy_Cli_RunReplLine(scratch.arena, String8_FromCStr(line), &output, &error);
        if (output.len > 0)
        {
            fwrite(output.data, 1, (size_t)output.len, stdout);
        }
        if (status != Memmy_Status_Ok)
        {
            fprintf(stderr, "memmy: %s", Memmy_Status_Name(status));
            if (error.message.len > 0)
            {
                fprintf(stderr, ": %.*s", (int)error.message.len, (char *)error.message.data);
            }
            fprintf(stderr, "\n");
        }
        if (result == Memmy_Status_Ok && status != Memmy_Status_Ok)
        {
            result = status;
        }

        String8 trimmed = String8_TrimWhitespace(String8_FromCStr(line));
        Scratch_End(scratch);
        if (String8_Eq(trimmed, String8_Lit("quit")) || String8_Eq(trimmed, String8_Lit("exit")))
        {
            break;
        }
    }
    return result;
}

int main(int argc, char **argv)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Error error = {0};
    Memmy_Context ctx = {0};
    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
    if (status == Memmy_Status_Ok)
    {
        Memmy_Context_Set(&ctx);
    }

    if (argc == 1)
    {
        if (status != Memmy_Status_Ok)
        {
            fprintf(stderr, "memmy: %s", Memmy_Status_Name(status));
            if (error.message.len > 0)
            {
                fprintf(stderr, ": %.*s", (int)error.message.len, (char *)error.message.data);
            }
            fprintf(stderr, "\n");
        }
        else
        {
            status = Memmy_Main_RunRepl(arena);
        }
        Memmy_Context_Set(0);
        Arena_Destroy(arena);
        return Memmy_Cli_ExitCodeFromStatus(status);
    }

    String8 output = {0};
    B32 json = Memmy_Cli_ArgvHasJson(argc, argv);
    B32 jsonl = Memmy_Cli_ArgvHasJsonl(argc, argv);
    status = Memmy_Cli_RunToString(arena, argc, argv, &output, &error);
    if (status != Memmy_Status_Ok && json)
    {
        output = Memmy_Cli_FormatJsonError(arena, &error);
    }
    else if (status != Memmy_Status_Ok && jsonl)
    {
        output = Memmy_Cli_FormatJsonlError(arena, &error);
    }
    if (output.len > 0)
    {
        fwrite(output.data, 1, (size_t)output.len, stdout);
    }

    if (status != Memmy_Status_Ok && !json && !jsonl)
    {
        fprintf(stderr, "memmy: %s", Memmy_Status_Name(status));
        if (error.message.len > 0)
        {
            fprintf(stderr, ": %.*s", (int)error.message.len, (char *)error.message.data);
        }
        fprintf(stderr, "\n");
    }

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
    return Memmy_Cli_ExitCodeFromStatus(status);
}
