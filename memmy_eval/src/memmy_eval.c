#include "memmy_eval.h"

Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena)
{
    if (arena == 0)
    {
        return 0;
    }

    Memmy_EvalEnv *env = Arena_PushStruct(arena, Memmy_EvalEnv);
    env->arena = arena;
    return env;
}

Memmy_Status Memmy_EvalStatement(Memmy_EvalEnv *env, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                 Memmy_Error *error)
{
    Unused(env);
    Unused(statement);
    Unused(sink);
    Unused(error);

    return Memmy_Status_Unsupported;
}

Memmy_Status Memmy_EvalExpr(Memmy_EvalEnv *env, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    Unused(env);
    Unused(expr);
    Unused(error);

    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }

    return Memmy_Status_Unsupported;
}

Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value)
{
    Unused(env);
    Unused(name);
    Unused(value);

    return Memmy_Status_Unsupported;
}

Memmy_Status Memmy_EvalEnv_Find(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue *out)
{
    Unused(env);
    Unused(name);

    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }

    return Memmy_Status_NotFound;
}

Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name)
{
    Unused(env);
    Unused(name);

    return Memmy_Status_Unsupported;
}

void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env)
{
    Unused(env);
}
