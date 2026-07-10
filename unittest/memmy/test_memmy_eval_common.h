#ifndef TEST_MEMMY_EVAL_COMMON_H
#define TEST_MEMMY_EVAL_COMMON_H

#include "base_memory.h"
#include "memmy_eval.h"
#include "test_framework.h"
#include "test_memmy_backend.h"
#include "test_memmy_common.h"

// Evaluator tests use their local arena for caller-owned result storage.
#define Memmy_EvalExpr(env, expr, out, error) Memmy_EvalExpr(arena, env, expr, out, error)
#define Memmy_EvalStatement(env, statement, sink, error) Memmy_EvalStatement(arena, env, statement, sink, error)
#define Memmy_EvalEnv_Find(env, name, out) Memmy_EvalEnv_Find(arena, env, name, out)

static void Test_EvalParseExpr(Arena *arena, char *text, Memmy_AstNode **out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
    AssertTrue(*out != 0);
}

static void Test_EvalParseStatement(Arena *arena, char *text, Memmy_AstStatement *out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseStatement(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
}

static void Test_EvalExprText(Memmy_EvalEnv *env, Arena *arena, char *text, Memmy_EvalValue *out)
{
    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, text, &expr);
    AssertEq(Memmy_EvalExpr(env, expr, out, 0), Memmy_Status_Ok);
}

static void Test_EvalStatementText(Memmy_EvalEnv *env, Arena *arena, char *text)
{
    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    AssertEq(Memmy_EvalStatement(env, &statement, 0, 0), Memmy_Status_Ok);
}

typedef struct Test_EvalResultCapture Test_EvalResultCapture;
struct Test_EvalResultCapture
{
    Memmy_EvalResult result;
    Memmy_EvalResult results[16];
    Memmy_EvalValue value;
    U64 count;
};

static Memmy_Status Test_EvalResultCapture_Push(void *user_data, Memmy_EvalResult const *result)
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

static void Test_EvalStatementResult(Memmy_EvalEnv *env, Arena *arena, char *text, Memmy_EvalValue *out)
{
    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {
        .callback = Test_EvalResultCapture_Push,
        .user_data = &capture,
    };
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    *out = capture.value;
}

static void Test_EvalStatementFullResult(Memmy_EvalEnv *env, Arena *arena, char *text, Memmy_EvalResult *out)
{
    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {
        .callback = Test_EvalResultCapture_Push,
        .user_data = &capture,
    };
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    *out = capture.result;
}

static void Test_EvalEnvWithProcess(Arena *arena, Test_MemmyBackend *backend, Memmy_EvalEnv **out)
{
    Test_MemmyBackend_Init(backend);
    Memmy_Context *ctx = Arena_PushStruct(arena, Memmy_Context);
    ctx->backend = Test_MemmyBackend_AsBackend(backend);
    Memmy_Context_Set(ctx);

    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_EvalEnv_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
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
