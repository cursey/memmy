#include "memmy_eval.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

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
    Memmy_EvalValue value;
    U64 count;
};

static void Test_EvalResultCapture_Push(Memmy_EvalResultSink *sink, Memmy_EvalResult result)
{
    Test_EvalResultCapture *capture = (Test_EvalResultCapture *)sink->user_data;
    capture->value = result.value;
    capture->count++;
}

static void Test_EvalStatementResult(Memmy_EvalEnv *env, Arena *arena, char *text, Memmy_EvalValue *out)
{
    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, text, &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {
        .push = Test_EvalResultCapture_Push,
        .user_data = &capture,
    };
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    *out = capture.value;
}

Test(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_EvalValue value = {.kind = Memmy_EvalValueKind_Const, .constant = 42};
    Memmy_EvalValue found = {0};

    AssertTrue(env != 0);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("x"), value), Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("x"), &found), Memmy_Status_Ok);
    AssertEq(found.kind, Memmy_EvalValueKind_Const);
    AssertEq(found.constant, 42);
    AssertEq(Memmy_EvalEnv_Unset(env, String8_Lit("x")), Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("x"), &found), Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalExpressionStatementsEmitResults)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Memmy_EvalValue constant = {0};
    Test_EvalStatementResult(env, arena, "40 + 2", &constant);
    AssertEq(constant.kind, Memmy_EvalValueKind_Const);
    AssertEq(constant.constant, 42);

    Memmy_EvalValue address = {0};
    Test_EvalStatementResult(env, arena, "@0x1000", &address);
    AssertEq(address.kind, Memmy_EvalValueKind_Address);
    AssertEq(address.address, 0x1000);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalConstVariablesBindImmediately)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$foo = 40 + 2");
    Test_EvalStatementText(env, arena, "$bar = $foo");
    Test_EvalStatementText(env, arena, "$foo = 100");

    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$bar + 1", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Const);
    AssertEq(value.constant, 43);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressVariablesAndArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$base = @0x1000");
    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$base + 0x20", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Address);
    AssertEq(value.address, 0x1020);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalRangeVariables)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$range = [@0x1000..+0x40]");
    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$range", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Range);
    AssertEq(value.range.start, 0x1000);
    AssertEq(value.range.end, 0x1040);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalWrongKindVariableInConstExpressionFails)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$range = [@0x1000..+0x40]");
    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "$range + 1", &expr);
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("expr"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalImmediateBindingAvoidsVariableCycles)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$a = 1");
    Test_EvalStatementText(env, arena, "$b = $a");
    Test_EvalStatementText(env, arena, "$a = $b + 1");

    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$a + $b", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Const);
    AssertEq(value.constant, 3);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalOverflowFails)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Memmy_EvalValue base = {.kind = Memmy_EvalValueKind_Address, .address = U64_MAX};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("base"), base), Memmy_Status_Ok);

    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "$base + 1", &expr);
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTypedIntegerVariablesWorkAsConstants)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    U8 bytes[] = {7, 0, 0, 0};
    Memmy_EvalValue typed = {
        .kind = Memmy_EvalValueKind_TypedValue,
        .constant = 7,
        .typed_value = {.type = {.kind = Memmy_TypeKind_I32, .fixed_size = 4},
                        .bytes = {.data = bytes, .len = ArrayCount(bytes)}},
    };

    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("n"), typed), Memmy_Status_Ok);
    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$n + 5", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Const);
    AssertEq(value.constant, 12);

    Memmy_EvalValue found = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("n"), &found), Memmy_Status_Ok);
    AssertEq(found.kind, Memmy_EvalValueKind_TypedValue);
    AssertEq(found.typed_value.type.kind, Memmy_TypeKind_I32);
    AssertTrue(found.typed_value.bytes.data != bytes);
    AssertEq(found.typed_value.bytes.len, 4);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalModuleAndProcessTargetsResolveToRanges)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, &process, 0), Memmy_Status_Ok);
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    env->process = process;

    Memmy_EvalValue module = {0};
    Test_EvalExprText(env, arena, "<test-module.exe>", &module);
    AssertEq(module.kind, Memmy_EvalValueKind_Range);
    AssertEq(module.range.start, 0x10000000);
    AssertEq(module.range.end, 0x10002000);

    Memmy_EvalValue process_range = {0};
    Test_EvalExprText(env, arena, "<4242!>", &process_range);
    AssertEq(process_range.kind, Memmy_EvalValueKind_Range);
    AssertEq(process_range.range.start, backend.memory_base);
    AssertEq(process_range.range.end, backend.memory_base + TEST_MEMMY_BACKEND_MEMORY_SIZE);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalMissingProcessDiagnostics)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Memmy_AstNode *target = 0;
    Test_EvalParseExpr(arena, "<client.dll>", &target);
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_EvalExpr(env, target, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("target"));

    Memmy_AstNode *deref = 0;
    Test_EvalParseExpr(arena, "@0x1000->", &deref);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, deref, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("address"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval = TestSuite_Make(
    "Memmy Eval", TestCase_Make(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues),
    TestCase_Make(Test_MemmyEvalExpressionStatementsEmitResults),
    TestCase_Make(Test_MemmyEvalConstVariablesBindImmediately),
    TestCase_Make(Test_MemmyEvalAddressVariablesAndArithmetic), TestCase_Make(Test_MemmyEvalRangeVariables),
    TestCase_Make(Test_MemmyEvalWrongKindVariableInConstExpressionFails),
    TestCase_Make(Test_MemmyEvalImmediateBindingAvoidsVariableCycles), TestCase_Make(Test_MemmyEvalOverflowFails),
    TestCase_Make(Test_MemmyEvalTypedIntegerVariablesWorkAsConstants),
    TestCase_Make(Test_MemmyEvalModuleAndProcessTargetsResolveToRanges),
    TestCase_Make(Test_MemmyEvalMissingProcessDiagnostics), );
