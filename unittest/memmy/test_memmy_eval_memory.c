#include "test_memmy_eval_common.h"

Test(Test_MemmyEvalReadsAndScansEmitOrdinaryValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    MemmyEval_Result read = {0};
    Test_EvalStatementFullResult(env, arena, "@0x1004 as u32", &read);
    AssertEq(read.kind, MemmyEval_ResultKind_Value);
    AssertTrue(Memmy_Type_Eq(read.value.type, Memmy_Type_U32));
    AssertEq(read.value.unsigned_integer, 0x07060504);

    MemmyEval_Result matches = {0};
    Test_EvalStatementFullResult(env, arena, "[@0x1000..+0x40]{10 11}", &matches);
    AssertEq(matches.kind, MemmyEval_ResultKind_Value);
    AssertTrue(Memmy_Type_IsList(matches.value.type));
    AssertTrue(Memmy_Type_Eq(*matches.value.type.list.element_type, Memmy_Type_Address));
    AssertEq(matches.value.list.count, 2);
    AssertEq(matches.value.list.addresses[0], 0x1010);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTypedReadsDecodeWithoutProvenance)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    Memmy_Value read = {0};
    Test_EvalExprText(env, arena, "@0x1004 as u32", &read);
    AssertTrue(Memmy_Type_Eq(read.type, Memmy_Type_U32));
    AssertEq(read.unsigned_integer, 0x07060504);

    backend.memory[0x40] = 'h';
    backend.memory[0x41] = 'i';
    backend.memory[0x42] = 0;
    Memmy_Value text = {0};
    Test_EvalExprText(env, arena, "@0x1040 as str", &text);
    AssertTrue(Memmy_Type_Eq(text.type, Memmy_Type_Str));
    AssertStrEq(text.string, String8_Lit("hi"));

    Test_EvalStatementText(env, arena, "$saved = @0x1004 as u32");
    Memmy_Value saved = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("saved"), &saved), Memmy_Status_Ok);
    AssertTrue(Memmy_Type_Eq(saved.type, Memmy_Type_U32));
    AssertEq(saved.unsigned_integer, 0x07060504);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTypedWriteSyntaxIsRejected)
{
    Arena *arena = Arena_CreateDefault();
    char *writes[] = {
        "@0x1004 as u32 = 0x11223344",
        "@0x1048 as str = \"hello\"",
    };
    for (U64 i = 0; i < ArrayCount(writes); i++)
    {
        MemmyAst_Node *expr = 0;
        MemmyAst_Diagnostic diagnostic = {0};
        AssertEq(MemmyAst_Expr_Parse(arena, String8_FromCStr(writes[i]), &expr, &diagnostic),
                 MemmyAst_Status_ParseError);
    }
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalParenthesizedTypedReadsInAddressArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x07] = 5;
    backend.memory[0x08] = 0;
    backend.memory[0x09] = 0;
    backend.memory[0x0a] = 0;
    Test_EvalStatementText(env, arena, "$entity_list_mov = @0x1004");

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$entity_list_mov + 7 + ($entity_list_mov + 3 as i32)", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x1010);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_memory =
    TestSuite_Make("Memmy Eval Memory", TestCase_Make(Test_MemmyEvalReadsAndScansEmitOrdinaryValues),
                   TestCase_Make(Test_MemmyEvalTypedReadsDecodeWithoutProvenance),
                   TestCase_Make(Test_MemmyEvalTypedWriteSyntaxIsRejected),
                   TestCase_Make(Test_MemmyEvalParenthesizedTypedReadsInAddressArithmetic), );
