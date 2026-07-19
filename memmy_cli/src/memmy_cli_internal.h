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

typedef struct MemmyCli_ValueFormat MemmyCli_ValueFormat;
struct MemmyCli_ValueFormat
{
    Memmy_Type type;
    String8 type_text;
};

typedef struct MemmyCli_PeekOutput MemmyCli_PeekOutput;
struct MemmyCli_PeekOutput
{
    Memmy_PointerWidth pointer_width;
    Memmy_Addr address;
    Memmy_Type type;
    String8 type_text;
    String8 bytes;
};

typedef struct MemmyCli_PokeOutput MemmyCli_PokeOutput;
struct MemmyCli_PokeOutput
{
    U32 pid;
    Memmy_PointerWidth pointer_width;
    Memmy_Addr address;
    Memmy_Type type;
    String8 type_text;
    String8 old_bytes;
    String8 new_bytes;
    B32 dry_run;
};

typedef struct MemmyCli_ScanOutput MemmyCli_ScanOutput;
struct MemmyCli_ScanOutput
{
    Arena *arena;
    MemmyCli_OutputWriter writer;
    Memmy_PointerWidth pointer_width;
    B32 jsonl;
    U64 count;
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

Memmy_Status MemmyCli_Value_Format(Arena *arena, MemmyCli_ValueFormat *format, String8 bytes, String8 *out,
                                   Memmy_Error *error);
String8 MemmyCli_Type_String(Memmy_Type type);
Memmy_Status MemmyCli_PeekOutput_Format(Arena *arena, MemmyCli_PeekOutput *peek, B32 jsonl, String8 *out,
                                        Memmy_Error *error);
Memmy_Status MemmyCli_PokeOutput_Format(Arena *arena, MemmyCli_PokeOutput *poke, B32 jsonl, String8 *out,
                                        Memmy_Error *error);
Memmy_Status MemmyCli_ScanOutput_Begin(MemmyCli_ScanOutput *output, Arena *arena, MemmyCli_OutputWriter writer,
                                       Memmy_PointerWidth pointer_width, B32 jsonl);
Memmy_Status MemmyCli_ScanOutput_PushMatch(void *user_data, Memmy_Addr address);
Memmy_Status MemmyCli_ScanOutput_End(MemmyCli_ScanOutput *output);

MemmyCli_Tutorial *MemmyCli_Tutorial_Create(Arena *arena);
Memmy_Status MemmyCli_Tutorial_Command_Run(Arena *arena, MemmyCli_Tutorial *tutorial, String8 argument, String8 *out,
                                           Memmy_Error *error);
MemmyEval_ResultSink const *MemmyCli_Tutorial_Statement_Begin(MemmyCli_Tutorial *tutorial);
String8 MemmyCli_Tutorial_Statement_End(Arena *arena, MemmyCli_Tutorial *tutorial, MemmyAst_Statement const *statement,
                                        Memmy_Status status, B32 has_attached_process, U32 attached_pid,
                                        MemmyEval_Env *env);

#endif // MEMMY_CLI_INTERNAL_H
