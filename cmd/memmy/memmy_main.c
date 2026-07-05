#include <stdio.h>

#include "base_os.h"
#include "memmy.h"
#include "memmy_cli.h"

static void Memmy_Main_WriteError(Arena *arena, Memmy_Status status, Memmy_Error *error)
{
    String8 message = String8_PushF(arena, "memmy: %s", Memmy_Status_Name(status));
    Os_WriteStderr(message);
    if (error != 0 && error->message.len > 0)
    {
        String8 detail = String8_PushF(arena, ": %.*s", (int)error->message.len, (char *)error->message.data);
        Os_WriteStderr(detail);
    }
    Os_WriteStderr(String8_Lit("\n"));
}

static Memmy_Status Memmy_Main_WriteStdout(void *user_data, String8 text)
{
    Unused(user_data);
    Os_WriteStdout(text);
    return Memmy_Status_Ok;
}

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
            Os_WriteStdout(output);
        }
        if (status != Memmy_Status_Ok)
        {
            Memmy_Main_WriteError(scratch.arena, status, &error);
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

    B32 stdin_is_terminal = Os_StdinIsTerminal();
    if (argc == 1 && stdin_is_terminal)
    {
        if (status != Memmy_Status_Ok)
        {
            Memmy_Main_WriteError(arena, status, &error);
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
    B32 jsonl = Memmy_Cli_ArgvHasJsonl(argc, argv);
    Memmy_CliOutputWriter stdout_writer = {
        .write = Memmy_Main_WriteStdout,
    };
    if (Memmy_Cli_ArgvHasHelp(argc, argv) || Memmy_Cli_ArgvHasVersion(argc, argv))
    {
        status = Memmy_Cli_RunToWriter(arena, argc, argv, stdout_writer, &error);
    }
    else if (!stdin_is_terminal)
    {
        String8 input = Os_ReadStdin(arena);
        if (input.len > 0)
        {
            status = Memmy_Cli_RunInputString(arena, argc, argv, input, &output, &error);
        }
        else
        {
            status = Memmy_Cli_RunToWriter(arena, argc, argv, stdout_writer, &error);
        }
    }
    else
    {
        status = Memmy_Cli_RunToWriter(arena, argc, argv, stdout_writer, &error);
    }
    if (status != Memmy_Status_Ok && jsonl)
    {
        output = Memmy_Cli_FormatJsonlError(arena, &error);
    }
    if (output.len > 0)
    {
        Os_WriteStdout(output);
    }

    if (status != Memmy_Status_Ok && !jsonl)
    {
        Memmy_Main_WriteError(arena, status, &error);
    }

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
    return Memmy_Cli_ExitCodeFromStatus(status);
}
