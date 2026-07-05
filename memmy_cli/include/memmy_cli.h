#ifndef MEMMY_CLI_H
#define MEMMY_CLI_H

#include "memmy.h"

typedef Memmy_Status Memmy_CliWriteFn(void *user_data, String8 text);

typedef struct Memmy_CliOutputWriter Memmy_CliOutputWriter;
struct Memmy_CliOutputWriter
{
    Memmy_CliWriteFn *write;
    void *user_data;
};

Memmy_Status Memmy_Cli_RunReplLine(Arena *arena, String8 line, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunReplString(Arena *arena, String8 input, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunInputString(Arena *arena, I32 argc, char **argv, String8 input, String8 *out,
                                      Memmy_Error *error);
Memmy_Status Memmy_Cli_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunToWriter(Arena *arena, I32 argc, char **argv, Memmy_CliOutputWriter writer,
                                   Memmy_Error *error);
I32 Memmy_Cli_ExitCodeFromStatus(Memmy_Status status);
B32 Memmy_Cli_ArgvHasHelp(I32 argc, char **argv);
B32 Memmy_Cli_ArgvHasVersion(I32 argc, char **argv);
B32 Memmy_Cli_ArgvHasJsonl(I32 argc, char **argv);
String8 Memmy_Cli_FormatAddress(Arena *arena, Memmy_PointerWidth pointer_width, Memmy_Addr address);
String8 Memmy_Cli_FormatJsonString(Arena *arena, String8 text);
String8 Memmy_Cli_FormatHexBytes(Arena *arena, String8 bytes);
String8 Memmy_Cli_FormatJsonlError(Arena *arena, Memmy_Error *error);

#endif // MEMMY_CLI_H
