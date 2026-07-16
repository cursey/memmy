#include <stdio.h>

#include "base.h"
#include "memmy.h"
#include "memmy_cli.h"

static void MemmyCli_Main_WriteError(Arena *arena, Memmy_Status status, Memmy_Error *error)
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

static Memmy_Status MemmyCli_Main_WriteStdout(void *user_data, String8 text)
{
    Unused(user_data);
    Os_WriteStdout(text);
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyCli_Main_RunRepl(Arena *arena)
{
    char line[4096];
    Memmy_Status result = Memmy_Status_Ok;
    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    B32 separate_next_prompt = 0;
    for (;;)
    {
        if (separate_next_prompt)
        {
            Os_WriteStdout(String8_Lit("\n"));
            separate_next_prompt = 0;
        }
        String8 prompt = MemmyCli_ReplSession_Prompt(arena, &session);
        Os_WriteStdout(prompt);
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == 0)
        {
            break;
        }

        Memmy_Error error = {0};
        String8 output = {0};
        B32 should_exit = 0;
        Memmy_Status status =
            MemmyCli_ReplSession_RunLine(arena, &session, String8_FromCStr(line), &output, &should_exit, &error);
        if (output.len > 0)
        {
            Os_WriteStdout(output);
            separate_next_prompt = 1;
        }
        if (status != Memmy_Status_Ok)
        {
            MemmyCli_Main_WriteError(arena, status, &error);
            separate_next_prompt = 1;
        }
        if (result == Memmy_Status_Ok && status != Memmy_Status_Ok)
        {
            result = status;
        }

        if (should_exit)
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
            MemmyCli_Main_WriteError(arena, status, &error);
        }
        else
        {
            status = MemmyCli_Main_RunRepl(arena);
        }
        Memmy_Context_Set(0);
        Arena_Destroy(arena);
        return MemmyCli_ExitCode_FromStatus(status);
    }

    String8 output = {0};
    B32 jsonl = MemmyCli_Argv_HasJsonl(argc, argv);
    MemmyCli_OutputWriter stdout_writer = {
        .write = MemmyCli_Main_WriteStdout,
    };
    if (MemmyCli_Argv_HasHelp(argc, argv) || MemmyCli_Argv_HasVersion(argc, argv))
    {
        status = MemmyCli_Argv_RunToWriter(arena, argc, argv, stdout_writer, &error);
    }
    else if (!stdin_is_terminal)
    {
        String8 input = Os_ReadStdin(arena);
        if (input.len > 0)
        {
            status = MemmyCli_Input_RunString(arena, argc, argv, input, &output, &error);
        }
        else
        {
            status = MemmyCli_Argv_RunToWriter(arena, argc, argv, stdout_writer, &error);
        }
    }
    else
    {
        status = MemmyCli_Argv_RunToWriter(arena, argc, argv, stdout_writer, &error);
    }
    if (status != Memmy_Status_Ok && jsonl)
    {
        output = MemmyCli_JsonlError_Format(arena, &error);
    }
    if (output.len > 0)
    {
        Os_WriteStdout(output);
    }

    if (status != Memmy_Status_Ok && !jsonl)
    {
        MemmyCli_Main_WriteError(arena, status, &error);
    }

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
    return MemmyCli_ExitCode_FromStatus(status);
}
