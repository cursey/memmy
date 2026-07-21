#include "test_memmy_eval_common.h"

static Memmy_Value Test_I64(I64 value)
{
    Memmy_Value result = {.type = Memmy_Type_I64, .signed_integer = value};
    return result;
}

static Memmy_Value Test_Address(Memmy_Addr value)
{
    Memmy_Value result = {.type = Memmy_Type_Address, .address = value};
    return result;
}

Test(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value found = {0};

    AssertTrue(env != 0);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("x"), Test_I64(42)), Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("x"), &found), Memmy_Status_Ok);
    AssertTrue(Memmy_Type_Eq(found.type, Memmy_Type_I64));
    AssertEq(found.signed_integer, 42);
    AssertEq(MemmyEval_Env_Unset(env, String8_Lit("x")), Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("x"), &found), Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalExpressionStatementsEmitSemanticValues)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Result result = {0};

    Test_EvalStatementFullResult(env, arena, "40 + 2", &result);
    AssertEq(result.kind, MemmyEval_ResultKind_Value);
    AssertTrue(Memmy_Type_Eq(result.value.type, Memmy_Type_I64));
    AssertEq(result.value.signed_integer, 42);

    Test_EvalStatementFullResult(env, arena, "@0x1000", &result);
    AssertEq(result.kind, MemmyEval_ResultKind_Value);
    AssertTrue(Memmy_Type_Eq(result.value.type, Memmy_Type_Address));
    AssertEq(result.value.address, 0x1000);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalVariablesBindIndependentSemanticCopies)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);

    Test_EvalStatementText(env, arena, "$foo = 40 + 2");
    Test_EvalStatementText(env, arena, "$bar = $foo");
    Test_EvalStatementText(env, arena, "$foo = 100");

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$bar + 1", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I64));
    AssertEq(value.signed_integer, 43);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressAndRangeVariables)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value value = {0};

    Test_EvalStatementText(env, arena, "$base = @0x1000");
    Test_EvalExprText(env, arena, "$base + 0x20", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x1020);

    Test_EvalStatementText(env, arena, "$range = [@0x1000..+0x40]");
    Test_EvalExprText(env, arena, "$range", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.start, 0x1000);
    AssertEq(value.range.end, 0x1040);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalWrongTypeInIntegerExpressionFails)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Test_EvalStatementText(env, arena, "$range = [@0x1000..+0x40]");
    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "$range * 1", &expr);
    Memmy_Value value = {0};
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("expr"));
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressOverflowFails)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("base"), Test_Address(U64_MAX)), Memmy_Status_Ok);
    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "$base + 1", &expr);
    Memmy_Value value = {0};
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressDifferenceBoundaries)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("zero"), Test_Address(0)), Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("max"), Test_Address((U64)I64_MAX)), Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("min_mag"), Test_Address((U64)I64_MAX + 1ull)), Memmy_Status_Ok);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$max - $zero", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I64));
    AssertEq(value.signed_integer, I64_MAX);
    Test_EvalExprText(env, arena, "$zero - $min_mag", &value);
    AssertEq(value.signed_integer, I64_MIN);

    Arena_Destroy(arena);
}

static B32 Test_PointerIsInArena(Arena *arena, void const *pointer)
{
    U8 const *bytes = (U8 const *)pointer;
    return bytes >= arena->base && bytes < arena->base + Arena_Pos(arena);
}

Test(Test_MemmyEvalResultsUseCallerOutputArena)
{
    Arena *env_arena = Arena_CreateDefault();
    Arena *parse_arena = Arena_CreateDefault();
    Arena *out_arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(env_arena);
    String8 source = String8_Lit("owned");
    Memmy_Value string_value = {.type = Memmy_Type_Str, .string = source};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("text"), string_value), Memmy_Status_Ok);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(parse_arena, "$text", &expr);
    Memmy_Value value = {0};
    AssertEq((MemmyEval_Expr_Eval)(out_arena, env, expr, &value, 0), Memmy_Status_Ok);
    AssertTrue(Test_PointerIsInArena(out_arena, value.string.data));
    AssertStrEq(value.string, source);

    Arena_Destroy(out_arena);
    Arena_Destroy(parse_arena);
    Arena_Destroy(env_arena);
}

TestSuite suite_memmy_eval_core_value = TestSuite_Make(
    "Memmy Eval Core And Value", TestCase_Make(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues),
    TestCase_Make(Test_MemmyEvalExpressionStatementsEmitSemanticValues),
    TestCase_Make(Test_MemmyEvalVariablesBindIndependentSemanticCopies),
    TestCase_Make(Test_MemmyEvalAddressAndRangeVariables),
    TestCase_Make(Test_MemmyEvalWrongTypeInIntegerExpressionFails), TestCase_Make(Test_MemmyEvalAddressOverflowFails),
    TestCase_Make(Test_MemmyEvalAddressDifferenceBoundaries),
    TestCase_Make(Test_MemmyEvalResultsUseCallerOutputArena), );
