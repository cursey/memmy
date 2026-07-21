#ifndef MEMMY_EVAL_H
#define MEMMY_EVAL_H

#include "memmy.h"
#include "memmy_ast.h"

typedef struct MemmyEval_Env MemmyEval_Env;

typedef U32 MemmyEval_ResultKind;
enum
{
    MemmyEval_ResultKind_Value,
    MemmyEval_ResultKind_Process,
    MemmyEval_ResultKind_Module,
    MemmyEval_ResultKind_Region,
    MemmyEval_ResultKind_Variable,
    MemmyEval_ResultKind_Unset,
    MemmyEval_ResultKind_Clear,
    MemmyEval_ResultKind_Help,
    MemmyEval_ResultKind_Exit,
};

typedef struct MemmyEval_VariableResult MemmyEval_VariableResult;
struct MemmyEval_VariableResult
{
    String8 name;
    Memmy_Value value;
};

typedef struct MemmyEval_Result MemmyEval_Result;
struct MemmyEval_Result
{
    MemmyEval_ResultKind kind;
    Memmy_Value value;
    Memmy_ProcessInfo process;
    Memmy_Module module;
    Memmy_Region region;
    MemmyEval_VariableResult variable;
    String8 name;
    String8 text;
};

typedef Memmy_Status MemmyEval_ResultSinkFn(void *user_data, MemmyEval_Result const *result);
typedef struct MemmyEval_ResultSink MemmyEval_ResultSink;
struct MemmyEval_ResultSink
{
    MemmyEval_ResultSinkFn *callback;
    void *user_data;
};

// The environment and all assigned bindings belong to arena. Values passed to Set are deep-copied.
MemmyEval_Env *MemmyEval_Env_Create(Arena *arena);
void MemmyEval_Env_SetDefaultProcess(MemmyEval_Env *env, U32 pid, Memmy_PointerWidth pointer_width);
void MemmyEval_Env_ClearDefaultProcess(MemmyEval_Env *env);
B32 MemmyEval_Env_GetDefaultProcess(MemmyEval_Env const *env, U32 *out_pid, Memmy_PointerWidth *out_pointer_width);
// Evaluation output data belongs to out_arena. AST inputs are borrowed and not modified.
// Sink result pointers and enumeration metadata are valid only for the callback duration.
Memmy_Status MemmyEval_Statement_Eval(Arena *out_arena, MemmyEval_Env *env, MemmyAst_Statement const *statement,
                                      MemmyEval_ResultSink const *sink, Memmy_Error *error);
Memmy_Status MemmyEval_Expr_Eval(Arena *out_arena, MemmyEval_Env *env, MemmyAst_Node const *expr, Memmy_Value *out,
                                 Memmy_Error *error);
Memmy_Status MemmyEval_Env_Set(MemmyEval_Env *env, String8 name, Memmy_Value value);
// Find deep-copies the binding value into out_arena and clears out on failure.
Memmy_Status MemmyEval_Env_Find(Arena *out_arena, MemmyEval_Env const *env, String8 name, Memmy_Value *out);
Memmy_Status MemmyEval_Env_Unset(MemmyEval_Env *env, String8 name);
void MemmyEval_Env_Clear(MemmyEval_Env *env);

#endif // MEMMY_EVAL_H
