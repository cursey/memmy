#ifndef MEMMY_CLI_INTERNAL_H
#define MEMMY_CLI_INTERNAL_H

#include "memmy_cli.h"

#include "memmy_eval.h"

typedef struct MemmyCli_Options MemmyCli_Options;
struct MemmyCli_Options
{
    String8 input_path;
    B32 help;
    B32 version;
    B32 jsonl;
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    B32 has_expr;
    String8 expr_text;
};

void MemmyCli_Line_Push(Arena *arena, String8List *list, char *fmt, ...);
Memmy_Status MemmyCli_Option_Invalid(Memmy_Error *error, String8 message, String8 input);
Memmy_Status MemmyCli_Repl_RunStringWithOptions(Arena *arena, MemmyCli_Options *base_options, String8 input,
                                                String8 *out, Memmy_Error *error);

Memmy_Status MemmyCli_Expr_RunToWriter(Arena *arena, MemmyCli_Options *options, MemmyCli_OutputWriter writer,
                                       Memmy_Error *error);
Memmy_Status MemmyCli_Expr_RunToWriterWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *options,
                                              MemmyCli_OutputWriter writer, Memmy_Error *error);
Memmy_Status MemmyCli_Statement_RunToWriterWithEnv(Arena *arena, MemmyEval_Env *env, MemmyCli_Options *options,
                                                   String8 text, MemmyCli_OutputWriter writer, B32 *out_exit,
                                                   MemmyEval_ResultSink const *observer, Memmy_Error *error);
Memmy_Status MemmyCli_ProcessInfo_Resolve(Arena *arena, B32 has_pid, U32 pid, B32 has_name, String8 name,
                                          Memmy_ProcessInfo *out, Memmy_Error *error);
Memmy_Status MemmyCli_Pid_ResolveOrOpenTransient(Arena *arena, U32 pid, Memmy_ProcessInfo *out, Memmy_Error *error);

MemmyCli_Tutorial *MemmyCli_Tutorial_Create(Arena *arena);
Memmy_Status MemmyCli_Tutorial_Command_Run(Arena *arena, MemmyCli_Tutorial *tutorial, String8 argument, String8 *out,
                                           Memmy_Error *error);
MemmyEval_ResultSink const *MemmyCli_Tutorial_Statement_Begin(MemmyCli_Tutorial *tutorial);
String8 MemmyCli_Tutorial_Statement_End(Arena *arena, MemmyCli_Tutorial *tutorial, MemmyAst_Statement const *statement,
                                        Memmy_Status status, B32 has_attached_process, U32 attached_pid,
                                        MemmyEval_Env *env);

#endif // MEMMY_CLI_INTERNAL_H
