#include "test_memmy_eval_common.h"

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
    Test_EvalParseExpr(arena, "$range * 1", &expr);
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

Test(Test_MemmyEvalAddressDifferenceBoundaries)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("zero"), (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address}),
             Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("max_i64"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = (U64)I64_MAX}),
             Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("min_i64_magnitude"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = (U64)I64_MAX + 1ull}),
             Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("too_large_magnitude"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = (U64)I64_MAX + 2ull}),
             Memmy_Status_Ok);

    Memmy_AstNode *expr = 0;
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};

    Test_EvalParseExpr(arena, "$max_i64 - $zero", &expr);
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, Memmy_EvalValueKind_Const);
    AssertEq(value.constant, I64_MAX);

    error = (Memmy_Error){0};
    Test_EvalParseExpr(arena, "$min_i64_magnitude - $zero", &expr);
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    error = (Memmy_Error){0};
    Test_EvalParseExpr(arena, "$zero - $min_i64_magnitude", &expr);
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, Memmy_EvalValueKind_Const);
    AssertEq(value.constant, I64_MIN);

    error = (Memmy_Error){0};
    Test_EvalParseExpr(arena, "$zero - $too_large_magnitude", &expr);
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

static B32 Test_PointerIsInArena(Arena *arena, void const *pointer)
{
    U8 const *bytes = (U8 const *)pointer;
    return bytes >= arena->base && bytes < arena->base + Arena_Pos(arena);
}

Test(Test_MemmyEvalResultsUseCallerOutputArena)
{
    AssertEq(Memmy_EvalResultKind_Null, 0);
    Arena *env_arena = Arena_CreateDefault();
    Arena *parse_arena = Arena_CreateDefault();
    Arena *out_one = Arena_CreateDefault();
    Arena *out_two = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(env_arena);
    Memmy_Addr source_addresses[] = {0x10, 0x20};
    U8 source_bytes[] = {7, 0, 0, 0};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("items"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList,
                                                 .addresses = source_addresses,
                                                 .address_count = ArrayCount(source_addresses)}),
             Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("typed"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_TypedValue,
                                                 .typed_value = {.type = {.kind = Memmy_TypeKind_U32},
                                                                 .bytes = String8_Make(source_bytes, 4)}}),
             Memmy_Status_Ok);

    Memmy_AstNode *items_expr = 0;
    Memmy_AstNode *typed_expr = 0;
    Test_EvalParseExpr(parse_arena, "$items", &items_expr);
    Test_EvalParseExpr(parse_arena, "$typed", &typed_expr);
    Memmy_EvalValue items = {0};
    Memmy_EvalValue typed = {0};
    AssertEq((Memmy_EvalExpr)(out_one, env, items_expr, &items, 0), Memmy_Status_Ok);
    AssertTrue(Test_PointerIsInArena(out_one, items.addresses));
    AssertEq((Memmy_EvalExpr)(out_two, env, typed_expr, &typed, 0), Memmy_Status_Ok);
    AssertTrue(Test_PointerIsInArena(out_two, typed.typed_value.bytes.data));
    AssertEq(items.addresses[0], 0x10);
    AssertEq(items.addresses[1], 0x20);

    Memmy_EvalValue found = {0};
    AssertEq((Memmy_EvalEnv_Find)(out_two, env, String8_Lit("items"), &found), Memmy_Status_Ok);
    AssertTrue(Test_PointerIsInArena(out_two, found.addresses));
    AssertTrue(found.addresses != items.addresses);

    Arena_Destroy(out_two);
    Arena_Destroy(out_one);
    Arena_Destroy(parse_arena);
    Arena_Destroy(env_arena);
}

TestSuite suite_memmy_eval_core_value = TestSuite_Make(
    "Memmy Eval Core And Value", TestCase_Make(Test_MemmyEvalCreatesEnvAndBindsArenaOwnedValues),
    TestCase_Make(Test_MemmyEvalExpressionStatementsEmitResults),
    TestCase_Make(Test_MemmyEvalConstVariablesBindImmediately),
    TestCase_Make(Test_MemmyEvalAddressVariablesAndArithmetic), TestCase_Make(Test_MemmyEvalRangeVariables),
    TestCase_Make(Test_MemmyEvalWrongKindVariableInConstExpressionFails),
    TestCase_Make(Test_MemmyEvalImmediateBindingAvoidsVariableCycles), TestCase_Make(Test_MemmyEvalOverflowFails),
    TestCase_Make(Test_MemmyEvalAddressDifferenceBoundaries),
    TestCase_Make(Test_MemmyEvalTypedIntegerVariablesWorkAsConstants),
    TestCase_Make(Test_MemmyEvalResultsUseCallerOutputArena), );
