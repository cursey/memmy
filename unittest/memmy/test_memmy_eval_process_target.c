#include "test_memmy_eval_common.h"

Test(Test_MemmyEvalModuleTargetAndProcessRangeResolveToSemanticValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "<test-module.exe>", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.start, 0x10000000);
    AssertEq(value.range.end, 0x10002000);

    Test_EvalExprText(env, arena, "[0..]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.start, 0);
    AssertEq(value.range.end, 0x1100);

    Test_EvalExprText(env, arena, "[0..] + 4", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 4);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalModuleAddressArithmeticUsesSemanticRules)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\test\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("a.dll"), String8_Lit("C:\\test\\a.dll"), 0x1000, 0x100);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x2000,
                                0x100);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("b.dll"), String8_Lit("C:\\test\\b.dll"), 0x2800, 0x100);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "<client.dll> + 0x123", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x2123);
    Test_EvalExprText(env, arena, "(<client.dll> + 0x123) - <client.dll>", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I64));
    AssertEq(value.signed_integer, 0x123);
    Test_EvalExprText(env, arena, "[<client.dll>+0x10..+0x20]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.start, 0x2010);
    AssertEq(value.range.end, 0x2030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalStoredAddressReadUsesCurrentSelectedProcess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("a.exe"), String8_Lit("C:\\test\\a.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&backend, 5678, String8_Lit("b.exe"), String8_Lit("C:\\test\\b.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x10000000, 0x100);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);
    Test_EvalStatementText(env, arena, "$addr = <client.dll>+0x4");

    Test_MemmyBackend_SetMemoryBase(&backend, 0x10000000);
    MemmyEval_Env_SetDefaultProcess(env, 5678, Memmy_PointerWidth_64);
    Memmy_Value read = {0};
    Test_EvalStatementResult(env, arena, "$addr as u32", &read);
    AssertTrue(Memmy_Type_Eq(read.type, Memmy_Type_U32));
    AssertEq(read.unsigned_integer, 0x07060504);
    AssertEq(backend.last_open_pid, 5678);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalProcessRangeIsFreshlyResolved)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);

    Memmy_Value first = {0};
    Memmy_Value second = {0};
    Test_EvalExprText(env, arena, "[0..]", &first);
    backend.backend.get_address_range = 0;
    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "[0..]", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &second, 0), Memmy_Status_Unsupported);
    AssertTrue(Memmy_Type_IsNull(second.type));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_process_target = TestSuite_Make(
    "Memmy Eval Process And Target", TestCase_Make(Test_MemmyEvalModuleTargetAndProcessRangeResolveToSemanticValues),
    TestCase_Make(Test_MemmyEvalModuleAddressArithmeticUsesSemanticRules),
    TestCase_Make(Test_MemmyEvalStoredAddressReadUsesCurrentSelectedProcess),
    TestCase_Make(Test_MemmyEvalProcessRangeIsFreshlyResolved), );
