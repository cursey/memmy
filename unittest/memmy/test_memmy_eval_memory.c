#include "test_memmy_eval_common.h"

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

    backend.memory[0x48] = 0;
    backend.memory[0x4d] = 0;
    Memmy_EvalResult string_write = {0};
    Test_EvalStatementFullResult(env, arena, "@0x1048 as str = \"hello\"", &string_write);
    AssertEq(string_write.kind, Memmy_EvalResultKind_Write);
    AssertStrEq(string_write.new_value.bytes, String8_Lit("hello"));
    Test_AssertBytes(String8_Make(&backend.memory[0x48], 5), (U8 *)"hello", 5);

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

Test(Test_MemmyEvalParenthesizedTypedReadsInAddressArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x07] = 5;
    backend.memory[0x08] = 0;
    backend.memory[0x09] = 0;
    backend.memory[0x0a] = 0;

    Test_EvalStatementText(env, arena, "$entity_list_mov = @0x1004");

    Memmy_EvalValue value = {0};
    Test_EvalExprText(env, arena, "$entity_list_mov + 7 + ($entity_list_mov + 3 as i32)", &value);
    AssertEq(value.kind, Memmy_EvalValueKind_Address);
    AssertEq(value.address, 0x1010);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_memory =
    TestSuite_Make("Memmy Eval Memory", TestCase_Make(Test_MemmyEvalStatementResultsClassifyReadsWritesAndAddressLists),
                   TestCase_Make(Test_MemmyEvalTypedReadsAndWritesWithFakeBackend),
                   TestCase_Make(Test_MemmyEvalTypedWriteReadsOldValueBeforeRhsEvaluation),
                   TestCase_Make(Test_MemmyEvalTypedWritePropagatesWriteFailuresAfterOldValueRead),
                   TestCase_Make(Test_MemmyEvalParenthesizedTypedReadsInAddressArithmetic), );
