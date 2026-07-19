#ifndef MEMMY_CLI_H
#define MEMMY_CLI_H

#include "memmy_eval.h"

typedef Memmy_Status MemmyCli_WriteFn(void *user_data, String8 text);

typedef struct MemmyCli_OutputWriter MemmyCli_OutputWriter;
struct MemmyCli_OutputWriter
{
    MemmyCli_WriteFn *write;
    void *user_data;
};

typedef struct MemmyCli_ReplSession MemmyCli_ReplSession;
typedef struct MemmyCli_Tutorial MemmyCli_Tutorial;
struct MemmyCli_ReplSession
{
    Arena *arena;
    MemmyEval_Env *env;
    MemmyCli_Tutorial *tutorial;
    B32 has_attached_process;
    Memmy_ProcessInfo attached_process;
};

MemmyCli_ReplSession MemmyCli_ReplSession_Begin(Arena *arena);
String8 MemmyCli_ReplSession_Prompt(Arena *arena, MemmyCli_ReplSession *session);
Memmy_Status MemmyCli_ReplSession_RunLine(Arena *arena, MemmyCli_ReplSession *session, String8 line, String8 *out,
                                          B32 *out_exit, Memmy_Error *error);
Memmy_Status MemmyCli_Repl_RunLine(Arena *arena, String8 line, String8 *out, Memmy_Error *error);
Memmy_Status MemmyCli_Repl_RunString(Arena *arena, String8 input, String8 *out, Memmy_Error *error);
Memmy_Status MemmyCli_Input_RunString(Arena *arena, I32 argc, char **argv, String8 input, String8 *out,
                                      Memmy_Error *error);
Memmy_Status MemmyCli_Argv_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error);
Memmy_Status MemmyCli_Argv_RunToWriter(Arena *arena, I32 argc, char **argv, MemmyCli_OutputWriter writer,
                                       Memmy_Error *error);
I32 MemmyCli_ExitCode_FromStatus(Memmy_Status status);
B32 MemmyCli_Argv_HasHelp(I32 argc, char **argv);
B32 MemmyCli_Argv_HasVersion(I32 argc, char **argv);
B32 MemmyCli_Argv_HasJsonl(I32 argc, char **argv);
String8 MemmyCli_Address_Format(Arena *arena, Memmy_PointerWidth pointer_width, Memmy_Addr address);
String8 MemmyCli_JsonString_Format(Arena *arena, String8 text);
String8 MemmyCli_HexBytes_Format(Arena *arena, String8 bytes);
String8 MemmyCli_JsonlError_Format(Arena *arena, Memmy_Error *error);

#endif // MEMMY_CLI_H
