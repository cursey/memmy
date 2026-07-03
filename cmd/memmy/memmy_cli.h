#ifndef MEMMY_CLI_H
#define MEMMY_CLI_H

#include "memmy.h"

Memmy_Status Memmy_Cli_ParseRangeOptions(I32 argc, char **argv, Memmy_Range *out, Memmy_Error *error);

#endif // MEMMY_CLI_H
