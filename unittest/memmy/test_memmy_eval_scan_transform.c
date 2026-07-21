#include "test_memmy_eval_common.h"

static void Test_AssertAddressList(Memmy_Value value, U64 count)
{
    AssertTrue(Memmy_Type_IsList(value.type));
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_Address));
    AssertEq(value.list.count, count);
}

Test(Test_MemmyEvalPatternScanAssignmentMaterializesSemanticList)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40]{10 11}");
    Memmy_Value matches = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("matches"), &matches), Memmy_Status_Ok);
    Test_AssertAddressList(matches, 2);
    AssertEq(matches.list.addresses[0], 0x1010);
    AssertEq(matches.list.addresses[1], 0x1020);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValueAndStringScansMaterializeSemanticLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x30] = 0x10;
    backend.memory[0x31] = 0x11;
    Memory_Copy(&backend.memory[0x40], "hello", 5);

    Memmy_Value matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as u16 == 0x1110", &matches);
    Test_AssertAddressList(matches, 2);
    AssertEq(matches.list.addresses[0], 0x1010);
    AssertEq(matches.list.addresses[1], 0x1030);

    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == hello", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1040);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalReferenceScansMaterializeSemanticLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU32LE(backend.memory + 0x20, 0x1040 - 0x1020 - 4);
    Test_EvalWriteU32LE(backend.memory + 0x30, (U32)(I32)(0x1040 - 0x1030 - 4));

    Memmy_Value matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs ptr @0x1040", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1010);
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs rel32 @0x1040", &matches);
    Test_AssertAddressList(matches, 2);
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs any @0x1040", &matches);
    Test_AssertAddressList(matches, 3);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalSemanticListsIndexTransformAndStore)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU64LE(backend.memory + 0x20, 0x1040);

    Test_EvalStatementText(env, arena, "$refs = [0..] refs ptr @0x1040");
    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$refs[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x1020);

    Test_EvalExprText(env, arena, "$refs => $ + 4", &value);
    Test_AssertAddressList(value, 2);
    AssertEq(value.list.addresses[0], 0x1014);
    AssertEq(value.list.addresses[1], 0x1024);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalNilShortCircuitsTransform)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "nil => $missing", &value);
    AssertTrue(Memmy_Type_IsNull(value.type));
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_scan_transform = TestSuite_Make(
    "Memmy Eval Scan And Transform", TestCase_Make(Test_MemmyEvalPatternScanAssignmentMaterializesSemanticList),
    TestCase_Make(Test_MemmyEvalValueAndStringScansMaterializeSemanticLists),
    TestCase_Make(Test_MemmyEvalReferenceScansMaterializeSemanticLists),
    TestCase_Make(Test_MemmyEvalSemanticListsIndexTransformAndStore),
    TestCase_Make(Test_MemmyEvalNilShortCircuitsTransform), );
