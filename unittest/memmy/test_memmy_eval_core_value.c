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

static void Test_EvalExpectStatus(MemmyEval_Env *env, Arena *arena, char *text, Memmy_Status expected);

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

Test(Test_MemmyEvalOrdinaryLiteralDefaultsAndConversions)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value value = {0};

    Test_EvalExprText(env, arena, "42.777", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_F64));
    F64 expected = 42.777;
    U64 expected_bits = 0;
    Memory_Copy(&expected_bits, &expected, sizeof(expected_bits));
    AssertEq(value.floating_bits, expected_bits);

    Test_EvalExprText(env, arena, "42.777 as f32", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_F32));
    Test_EvalExprText(env, arena, "\"hello\"", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Str));
    AssertStrEq(value.string, String8_Lit("hello"));
    Test_EvalExprText(env, arena, "\"hello\" as wstr", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_WStr));
    AssertStrEq(value.string, String8_Lit("hello"));
    Test_EvalExprText(env, arena, "42.9 as i32", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I32));
    AssertEq(value.signed_integer, 42);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "1.0 + 2.0", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, 0), Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalRejectsMalformedUtf8StringLiterals)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    U8 source[] = {'"', 0xc0, 0x80, '"'};
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_Make(source, sizeof(source)), &expr, &diagnostic), MemmyAst_Status_Ok);

    Memmy_Value value = {0};
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidEncoding);
    AssertTrue(Memmy_Type_IsNull(value.type));
    AssertStrEq(error.context, String8_Lit("string"));
    AssertEq(error.input.len, 0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalIntegerPromotionsAndUsualConversions)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("u8a"), (Memmy_Value){.type = Memmy_Type_U8, .unsigned_integer = 250}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("u8b"), (Memmy_Value){.type = Memmy_Type_U8, .unsigned_integer = 10}),
             Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("u32"), (Memmy_Value){.type = Memmy_Type_U32, .unsigned_integer = U32_MAX}),
        Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("i32"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = -1}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("u64"), (Memmy_Value){.type = Memmy_Type_U64, .unsigned_integer = 0}),
             Memmy_Status_Ok);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$u8a + $u8b", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I32));
    AssertEq(value.signed_integer, 260);
    Test_EvalExprText(env, arena, "$u32 + 1", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I64));
    AssertEq(value.signed_integer, (I64)U32_MAX + 1);
    Test_EvalExprText(env, arena, "$i32 + $u32", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_U32));
    AssertEq(value.unsigned_integer, U32_MAX - 1);
    Test_EvalExprText(env, arena, "$i32 + $u64", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_U64));
    AssertEq(value.unsigned_integer, U64_MAX);
    Test_EvalExprText(env, arena, "-$u8b", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I32));
    AssertEq(value.signed_integer, -10);
    Test_EvalExprText(env, arena, "$i32 as u64", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_U64));
    AssertEq(value.unsigned_integer, U64_MAX);
    Test_EvalExpectStatus(env, arena, "$u32 as i32", Memmy_Status_Overflow);
    Test_EvalExpectStatus(env, arena, "\"42\" as u64", Memmy_Status_InvalidArgument);
    Test_EvalExpectStatus(env, arena, "nil as u64", Memmy_Status_InvalidArgument);

    Arena_Destroy(arena);
}

static void Test_EvalExpectStatus(MemmyEval_Env *env, Arena *arena, char *text, Memmy_Status expected)
{
    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, text, &expr);
    Memmy_Value value = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, 0), expected);
}

Test(Test_MemmyEvalIntegerOverflowWrapAndDivisionRules)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("imax"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = I32_MAX}),
        Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("imin"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = I32_MIN}),
        Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("one"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = 1}),
             Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("minus_one"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = -1}),
        Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("umax"), (Memmy_Value){.type = Memmy_Type_U32, .unsigned_integer = U32_MAX}),
        Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("uone"), (Memmy_Value){.type = Memmy_Type_U32, .unsigned_integer = 1}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("zero"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = 0}),
             Memmy_Status_Ok);

    Test_EvalExpectStatus(env, arena, "$imax + $one", Memmy_Status_Overflow);
    Test_EvalExpectStatus(env, arena, "$imin / $minus_one", Memmy_Status_Overflow);
    Test_EvalExpectStatus(env, arena, "$one / $zero", Memmy_Status_InvalidArgument);
    Test_EvalExpectStatus(env, arena, "$one % $zero", Memmy_Status_InvalidArgument);
    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$umax + $uone", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_U32));
    AssertEq(value.unsigned_integer, 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressConstructionRejectsNegativeAndAcceptsU64)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("wide"), (Memmy_Value){.type = Memmy_Type_U64, .unsigned_integer = U64_MAX}),
        Memmy_Status_Ok);
    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "@$wide", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, U64_MAX);
    Test_EvalExpectStatus(env, arena, "@-1", Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressAndRangeProjectionMatrix)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("addr"), Test_Address(0x1000)), Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("range"),
                               (Memmy_Value){.type = Memmy_Type_Range, .range = {.start = 0x1000, .end = 0x1100}}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("neg"), (Memmy_Value){.type = Memmy_Type_I32, .signed_integer = -0x10}),
             Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("usize"), (Memmy_Value){.type = Memmy_Type_U64, .unsigned_integer = 0x20}),
        Memmy_Status_Ok);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$addr + $neg", &value);
    AssertEq(value.address, 0xff0);
    Test_EvalExprText(env, arena, "$addr - $neg", &value);
    AssertEq(value.address, 0x1010);
    Test_EvalExprText(env, arena, "1 + $addr", &value);
    AssertEq(value.address, 0x1001);
    Test_EvalExprText(env, arena, "$range + 4", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x1004);
    Test_EvalExprText(env, arena, "$range - $addr", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I64));
    AssertEq(value.signed_integer, 0);
    Test_EvalExprText(env, arena, "[$addr..+$usize]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.end, 0x1020);

    Test_EvalExpectStatus(env, arena, "1 - $addr", Memmy_Status_InvalidArgument);
    Test_EvalExpectStatus(env, arena, "$addr + $addr", Memmy_Status_InvalidArgument);
    Test_EvalExpectStatus(env, arena, "$range * 2", Memmy_Status_InvalidArgument);
    Test_EvalExpectStatus(env, arena, "[$addr..+$neg]", Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_core_value = TestSuite_Make(
    "Memmy Eval Core And Value", TestCase_Make(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues),
    TestCase_Make(Test_MemmyEvalExpressionStatementsEmitSemanticValues),
    TestCase_Make(Test_MemmyEvalVariablesBindIndependentSemanticCopies),
    TestCase_Make(Test_MemmyEvalAddressAndRangeVariables),
    TestCase_Make(Test_MemmyEvalWrongTypeInIntegerExpressionFails), TestCase_Make(Test_MemmyEvalAddressOverflowFails),
    TestCase_Make(Test_MemmyEvalAddressDifferenceBoundaries), TestCase_Make(Test_MemmyEvalResultsUseCallerOutputArena),
    TestCase_Make(Test_MemmyEvalOrdinaryLiteralDefaultsAndConversions),
    TestCase_Make(Test_MemmyEvalRejectsMalformedUtf8StringLiterals),
    TestCase_Make(Test_MemmyEvalIntegerPromotionsAndUsualConversions),
    TestCase_Make(Test_MemmyEvalIntegerOverflowWrapAndDivisionRules),
    TestCase_Make(Test_MemmyEvalAddressConstructionRejectsNegativeAndAcceptsU64),
    TestCase_Make(Test_MemmyEvalAddressAndRangeProjectionMatrix), );
