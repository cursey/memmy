#include "memmy_eval.h"
#include "test_framework.h"
#include "test_memmy_backend.h"
#include "test_memmy_common.h"

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

static void Test_EvalResultCapture_Push(Memmy_EvalResultSink *sink, Memmy_EvalResult result)
{
    Test_EvalResultCapture *capture = (Test_EvalResultCapture *)sink->user_data;
    capture->result = result;
    if (capture->count < ArrayCount(capture->results))
    {
        capture->results[capture->count] = result;
    }
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

static void Test_EvalStatementFullResult(Memmy_EvalEnv *env, Arena *arena, char *text, Memmy_EvalResult *out)
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
    *out = capture.result;
}

static void Test_EvalEnvWithProcess(Arena *arena, Test_MemmyBackend *backend, Memmy_EvalEnv **out)
{
    Test_MemmyBackend_Init(backend);
    Memmy_Context *ctx = Arena_PushStruct(arena, Memmy_Context);
    ctx->backend = Test_MemmyBackend_AsBackend(backend);
    Memmy_Context_Set(ctx);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, &process, 0), Memmy_Status_Ok);
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    env->process = process;
    *out = env;
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

Test(Test_MemmyEvalStatementResultsClassifyReadsWritesAndAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Memmy_EvalResult read = {0};
    Test_EvalStatementFullResult(env, arena, "@0x1004 as u32", &read);
    AssertEq(read.kind, Memmy_EvalResultKind_Read);
    AssertEq(read.address, 0x1004);
    AssertEq(read.value.kind, Memmy_EvalValueKind_TypedValue);

    Memmy_EvalResult matches = {0};
    Test_EvalStatementFullResult(env, arena, "[@0x1000..+0x40]{10 11}", &matches);
    AssertEq(matches.kind, Memmy_EvalResultKind_AddressList);
    AssertEq(matches.value.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(matches.value.address_count, 2);

    Memmy_EvalResult write = {0};
    Test_EvalStatementFullResult(env, arena, "@0x1004 as u32 = 0x11223344", &write);
    AssertEq(write.kind, Memmy_EvalResultKind_Write);
    AssertEq(write.address, 0x1004);

    Memmy_Context_Set(0);
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

Test(Test_MemmyEvalTypedReadsAndWritesWithFakeBackend)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    Memmy_EvalValue read = {0};
    Test_EvalExprText(env, arena, "@0x1004 as u32", &read);
    AssertEq(read.kind, Memmy_EvalValueKind_TypedValue);
    AssertEq(read.address, 0x1004);
    AssertEq(read.constant, 0x07060504);
    U8 old_bytes[] = {4, 5, 6, 7};
    Test_AssertBytes(read.typed_value.bytes, old_bytes, ArrayCount(old_bytes));

    backend.memory[0x40] = 'h';
    backend.memory[0x41] = 'i';
    backend.memory[0x42] = 0;
    Memmy_EvalValue text = {0};
    Test_EvalExprText(env, arena, "@0x1040 as str", &text);
    AssertEq(text.kind, Memmy_EvalValueKind_TypedValue);
    AssertStrEq(text.typed_value.bytes, String8_Lit("hi"));

    Memmy_EvalResult write = {0};
    Test_EvalStatementFullResult(env, arena, "@0x1004 as u32 = 0x11223344", &write);
    AssertEq(write.kind, Memmy_EvalResultKind_Write);
    AssertEq(write.address, 0x1004);
    Test_AssertBytes(write.old_value.bytes, old_bytes, ArrayCount(old_bytes));
    U8 new_bytes[] = {0x44, 0x33, 0x22, 0x11};
    Test_AssertBytes(write.new_value.bytes, new_bytes, ArrayCount(new_bytes));
    Test_AssertBytes(String8_Make(&backend.memory[4], 4), new_bytes, ArrayCount(new_bytes));

    Test_MemmyBackend_SetReadStatus(&backend, Memmy_Status_AccessDenied);
    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "@0x1008 as u32 = 1", &expr);
    Memmy_EvalValue value = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, 0), Memmy_Status_AccessDenied);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTypedWriteReadsOldValueBeforeRhsEvaluation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "@0x1004 as u32 = $missing_rhs", &expr);
    Memmy_EvalValue value = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, 0), Memmy_Status_NotFound);
    AssertEq(backend.read_call_count, 1);
    AssertEq(backend.min_read_addr, 0x1004);
    AssertEq(backend.max_read_end, 0x1008);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTypedWritePropagatesWriteFailuresAfterOldValueRead)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    Test_MemmyBackend_SetWriteStatus(&backend, Memmy_Status_AccessDenied);
    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "@0x1004 as u32 = 0x11223344", &expr);
    Memmy_EvalValue value = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, 0), Memmy_Status_AccessDenied);
    AssertEq(backend.read_call_count, 1);
    U8 old_bytes[] = {4, 5, 6, 7};
    Test_AssertBytes(String8_Make(&backend.memory[4], 4), old_bytes, ArrayCount(old_bytes));

    Test_MemmyBackend_SetWriteStatus(&backend, Memmy_Status_Ok);
    Test_MemmyBackend_SetWriteLimit(&backend, 2);
    backend.read_call_count = 0;
    Memmy_AstNode *partial_expr = 0;
    Test_EvalParseExpr(arena, "@0x1008 as u32 = 0x55667788", &partial_expr);
    AssertEq(Memmy_EvalExpr(env, partial_expr, &value, 0), Memmy_Status_PartialWrite);
    AssertEq(backend.read_call_count, 1);
    U8 partial_bytes[] = {0x88, 0x77, 10, 11};
    Test_AssertBytes(String8_Make(&backend.memory[8], 4), partial_bytes, ArrayCount(partial_bytes));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalPatternScanAssignmentMaterializesAddressList)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40]{10 11}");
    Memmy_EvalValue matches = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("matches"), &matches), Memmy_Status_Ok);
    AssertEq(matches.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(matches.address_count, 2);
    AssertEq(matches.addresses[0], 0x1010);
    AssertEq(matches.addresses[1], 0x1020);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValueScanAssignmentMaterializesAddressList)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x30] = 0x10;
    backend.memory[0x31] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40] as u16 == 0x1110");
    Memmy_EvalValue matches = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("matches"), &matches), Memmy_Status_Ok);
    AssertEq(matches.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(matches.address_count, 2);
    AssertEq(matches.addresses[0], 0x1010);
    AssertEq(matches.addresses[1], 0x1030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalIndexesAssignedAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40]{10 11}");
    Memmy_EvalValue second = {0};
    Test_EvalExprText(env, arena, "$matches[1]", &second);
    AssertEq(second.kind, Memmy_EvalValueKind_Address);
    AssertEq(second.address, 0x1020);

    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "$matches[2]", &expr);
    Memmy_Error error = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &second, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("index"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalIndexesValueScanExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x30] = 0x10;
    backend.memory[0x31] = 0x11;

    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "([@0x1000..+0x40] as u16 == 0x1110)[1]", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Address);
    AssertEq(value.address, 0x1030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAnchorTargetExampleFlow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0xaa;
    backend.memory[0x21] = 0xbb;
    backend.memory[0x24] = 0xef;
    backend.memory[0x25] = 0xbe;

    Test_EvalStatementText(env, arena, "$anchor = [@0x1000..+0x40]{aa bb}[0]");
    Test_EvalStatementText(env, arena, "$target = $anchor + 4");

    Memmy_EvalValue target = {0};
    Test_EvalExprText(env, arena, "$target", &target);
    AssertEq(target.kind, Memmy_EvalValueKind_Address);
    AssertEq(target.address, 0x1024);

    Memmy_EvalValue target_value = {0};
    Test_EvalExprText(env, arena, "$target as u16", &target_value);
    AssertEq(target_value.kind, Memmy_EvalValueKind_TypedValue);
    AssertEq(target_value.constant, 0xbeef);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalCommandsListProcessesModulesAndRegionsWithFuzzyFilters)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_MemmyBackend_AddProcess(&backend, 7777, String8_Lit("ClientGame.exe"), String8_Lit("C:\\game\\ClientGame.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client-main.dll"),
                                String8_Lit("C:\\test\\client-main.dll"), 0x20000000, 0x3000);
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("physics.dll"), String8_Lit("C:\\test\\physics.dll"),
                                0x30000000, 0x1000);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x2000, 0x80, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/procs CGe", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Process);
    AssertEq(capture.results[0].process.pid, 7777);

    Test_EvalParseStatement(arena, "/mods cmain", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Module);
    AssertStrEq(capture.results[0].module.name, String8_Lit("client-main.dll"));

    Test_EvalParseStatement(arena, "/regions", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 2);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Region);
    AssertEq(capture.results[1].kind, Memmy_EvalResultKind_Region);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalVarsUnsetAndClearCommands)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$foo = 42");
    Test_EvalStatementText(env, arena, "$bar = @0x1000");

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/vars", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 2);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Variable);
    AssertEq(capture.results[1].kind, Memmy_EvalResultKind_Variable);

    Test_EvalParseStatement(arena, "/unset $foo", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Unset);
    AssertStrEq(capture.results[0].name, String8_Lit("foo"));
    Memmy_EvalValue value = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("foo"), &value), Memmy_Status_NotFound);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("bar"), &value), Memmy_Status_Ok);

    Test_EvalParseStatement(arena, "/clear", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Clear);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("bar"), &value), Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalHelpAndExitCommandsEmitControlResults)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/help", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Help);
    AssertTrue(String8_Find(capture.results[0].text, String8_Lit("/mods [filter]"), 0) != STRING8_NPOS);

    Test_EvalParseStatement(arena, "/exit", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Exit);

    Test_EvalParseStatement(arena, "/quit", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Exit);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval = TestSuite_Make(
    "Memmy Eval", TestCase_Make(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues),
    TestCase_Make(Test_MemmyEvalExpressionStatementsEmitResults),
    TestCase_Make(Test_MemmyEvalStatementResultsClassifyReadsWritesAndAddressLists),
    TestCase_Make(Test_MemmyEvalConstVariablesBindImmediately),
    TestCase_Make(Test_MemmyEvalAddressVariablesAndArithmetic), TestCase_Make(Test_MemmyEvalRangeVariables),
    TestCase_Make(Test_MemmyEvalWrongKindVariableInConstExpressionFails),
    TestCase_Make(Test_MemmyEvalImmediateBindingAvoidsVariableCycles), TestCase_Make(Test_MemmyEvalOverflowFails),
    TestCase_Make(Test_MemmyEvalTypedIntegerVariablesWorkAsConstants),
    TestCase_Make(Test_MemmyEvalModuleAndProcessTargetsResolveToRanges),
    TestCase_Make(Test_MemmyEvalMissingProcessDiagnostics),
    TestCase_Make(Test_MemmyEvalTypedReadsAndWritesWithFakeBackend),
    TestCase_Make(Test_MemmyEvalTypedWriteReadsOldValueBeforeRhsEvaluation),
    TestCase_Make(Test_MemmyEvalTypedWritePropagatesWriteFailuresAfterOldValueRead),
    TestCase_Make(Test_MemmyEvalPatternScanAssignmentMaterializesAddressList),
    TestCase_Make(Test_MemmyEvalValueScanAssignmentMaterializesAddressList),
    TestCase_Make(Test_MemmyEvalIndexesAssignedAddressLists), TestCase_Make(Test_MemmyEvalIndexesValueScanExpressions),
    TestCase_Make(Test_MemmyEvalAnchorTargetExampleFlow),
    TestCase_Make(Test_MemmyEvalCommandsListProcessesModulesAndRegionsWithFuzzyFilters),
    TestCase_Make(Test_MemmyEvalVarsUnsetAndClearCommands),
    TestCase_Make(Test_MemmyEvalHelpAndExitCommandsEmitControlResults), );
