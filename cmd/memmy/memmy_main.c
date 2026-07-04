#include <stdio.h>

#include "memmy.h"
#include "memmy_cli.h"

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
