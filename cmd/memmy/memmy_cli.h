#ifndef MEMMY_CLI_H
#define MEMMY_CLI_H

#include "memmy.h"

Memmy_Status Memmy_Cli_ParseRangeOptions(I32 argc, char **argv, Memmy_Range *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error);
I32 Memmy_Cli_ExitCodeFromStatus(Memmy_Status status);

#endif // MEMMY_CLI_H
