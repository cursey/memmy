#ifndef MEMMY_EVAL_H
#define MEMMY_EVAL_H

#include "memmy.h"
#include "memmy_ast.h"

typedef U32 Memmy_EvalValueKind;
enum
{
    Memmy_EvalValueKind_Null,
    Memmy_EvalValueKind_Const,
    Memmy_EvalValueKind_Address,
    Memmy_EvalValueKind_Range,
    Memmy_EvalValueKind_AddressList,
    Memmy_EvalValueKind_TypedValue,
};

typedef struct Memmy_EvalEnv Memmy_EvalEnv;
struct Memmy_EvalEnv
{
    Arena *arena;
};

typedef struct Memmy_EvalValue Memmy_EvalValue;
struct Memmy_EvalValue
{
    Memmy_EvalValueKind kind;
    I64 constant;
    Memmy_Addr address;
    Memmy_Range range;
};

typedef struct Memmy_EvalResult Memmy_EvalResult;
struct Memmy_EvalResult
{
    Memmy_EvalValue value;
};

typedef struct Memmy_EvalResultSink Memmy_EvalResultSink;
struct Memmy_EvalResultSink
{
    void (*push)(Memmy_EvalResultSink *sink, Memmy_EvalResult result);
    void *user_data;
};

Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena);
Memmy_Status Memmy_EvalStatement(Memmy_EvalEnv *env, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                 Memmy_Error *error);
Memmy_Status Memmy_EvalExpr(Memmy_EvalEnv *env, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error);
Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value);
Memmy_Status Memmy_EvalEnv_Find(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue *out);
Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name);
void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env);

#endif // MEMMY_EVAL_H
