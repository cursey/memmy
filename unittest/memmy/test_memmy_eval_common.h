#ifndef TEST_MEMMY_EVAL_COMMON_H
#define TEST_MEMMY_EVAL_COMMON_H

#include "base.h"
#include "memmy_eval.h"
#include "test_framework.h"
#include "test_memmy_backend.h"
#include "test_memmy_common.h"

// Evaluator tests use their local arena for caller-owned result storage.
#define MemmyEval_Expr_Eval(env, expr, out, error) MemmyEval_Expr_Eval(arena, env, expr, out, error)
#define MemmyEval_Statement_Eval(env, statement, sink, error)                                                          \
    MemmyEval_Statement_Eval(arena, env, statement, sink, error)
#define MemmyEval_Env_Find(env, name, out) MemmyEval_Env_Find(arena, env, name, out)

static void Test_EvalParseExpr(Arena *arena, char *text, MemmyAst_Node **out)
{
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_FromCStr(text), out, &diagnostic), MemmyAst_Status_Ok);
    AssertTrue(*out != 0);
}

static void Test_EvalParseStatement(Arena *arena, char *text, MemmyAst_Statement *out)
{
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Statement_Parse(arena, String8_FromCStr(text), out, &diagnostic), MemmyAst_Status_Ok);
}

static void Test_EvalExprText(MemmyEval_Env *env, Arena *arena, char *text, MemmyEval_Value *out)
{
    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, text, &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, out, 0), Memmy_Status_Ok);
}

static void Test_EvalStatementText(MemmyEval_Env *env, Arena *arena, char *text)
{
    MemmyAst_Statement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    AssertEq(MemmyEval_Statement_Eval(env, &statement, 0, 0), Memmy_Status_Ok);
}

typedef struct Test_EvalResultCapture Test_EvalResultCapture;
struct Test_EvalResultCapture
{
    MemmyEval_Result result;
    MemmyEval_Result results[16];
    MemmyEval_Value value;
    U64 count;
};

static Memmy_Status Test_EvalResultCapture_Push(void *user_data, MemmyEval_Result const *result)
{
    Test_EvalResultCapture *capture = (Test_EvalResultCapture *)user_data;
    capture->result = *result;
    if (capture->count < ArrayCount(capture->results))
    {
        capture->results[capture->count] = *result;
    }
    capture->value = result->value;
    capture->count++;
    return Memmy_Status_Ok;
}

static void Test_EvalStatementResult(MemmyEval_Env *env, Arena *arena, char *text, MemmyEval_Value *out)
{
    MemmyAst_Statement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    Test_EvalResultCapture capture = {0};
    MemmyEval_ResultSink sink = {
        .callback = Test_EvalResultCapture_Push,
        .user_data = &capture,
    };
    AssertEq(MemmyEval_Statement_Eval(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    *out = capture.value;
}

static void Test_EvalStatementFullResult(MemmyEval_Env *env, Arena *arena, char *text, MemmyEval_Result *out)
{
    MemmyAst_Statement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    Test_EvalResultCapture capture = {0};
    MemmyEval_ResultSink sink = {
        .callback = Test_EvalResultCapture_Push,
        .user_data = &capture,
    };
    AssertEq(MemmyEval_Statement_Eval(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    *out = capture.result;
}

static void Test_EvalEnvWithProcess(Arena *arena, Test_MemmyBackend *backend, MemmyEval_Env **out)
{
    Test_MemmyBackend_Init(backend);
    Memmy_Context *ctx = Arena_PushStruct(arena, Memmy_Context);
    ctx->backend = Test_MemmyBackend_AsBackend(backend);
    Memmy_Context_Set(ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    *out = env;
}

static void Test_EvalWriteU32LE(U8 *dst, U32 value)
{
    for (U64 i = 0; i < 4; i++)
    {
        dst[i] = (U8)(value >> (i * 8));
    }
}

static void Test_EvalWriteU64LE(U8 *dst, U64 value)
{
    for (U64 i = 0; i < 8; i++)
    {
        dst[i] = (U8)(value >> (i * 8));
    }
}

static void Test_EvalConfigureObjectBase(Test_MemmyBackend *backend, U32 pid, Memmy_Addr object, Memmy_Addr vtable,
                                         Memmy_Addr code)
{
    for (U64 i = 0; i < backend->region_count; i++)
    {
        if (backend->regions[i].pid == pid)
        {
            backend->regions[i].access |= Memmy_RegionAccess_Execute;
        }
    }
    Test_EvalWriteU64LE(backend->memory + (object - backend->memory_base), vtable);
    Test_EvalWriteU64LE(backend->memory + (vtable - backend->memory_base), code);
    Test_EvalWriteU64LE(backend->memory + (vtable + 8 - backend->memory_base), code + 8);
}

#endif
