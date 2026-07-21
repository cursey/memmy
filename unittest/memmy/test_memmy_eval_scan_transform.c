#include "test_memmy_eval_common.h"

static void Test_AssertAddressList(Memmy_Value value, U64 count)
{
    AssertTrue(Memmy_Type_IsList(value.type));
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_Address));
    AssertEq(value.list.count, count);
}

static Memmy_Value Test_ListValue(Arena *arena, Memmy_Type element_type, U64 count)
{
    Memmy_Type type = {0};
    if (Memmy_Type_ListCreate(arena, element_type, &type, 0) != Memmy_Status_Ok)
    {
        return (Memmy_Value){0};
    }
    return (Memmy_Value){.type = type, .list.count = count};
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

    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == \"hello\"", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1040);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValueScansEvaluateAndConvertRhsExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x11;
    backend.memory[0x21] = 0x11;
    Test_EvalStatementText(env, arena, "$needle = 0x1110");

    Memmy_Value matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] as u16 == $needle + 1", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1020);

    F64 literal = 42.777;
    U64 literal_bits = 0;
    Memory_Copy(&literal_bits, &literal, sizeof(literal_bits));
    Memmy_Value f64 = {.type = Memmy_Type_F64, .floating_bits = literal_bits};
    Memmy_Value f32 = {0};
    AssertEq(Memmy_Value_Convert(arena, &f64, Memmy_Type_F32, &f32, 0), Memmy_Status_Ok);
    Memmy_EncodedValue encoded = {0};
    AssertEq(Memmy_Value_Encode(arena, &f32, &encoded, 0), Memmy_Status_Ok);
    Memory_Copy(&backend.memory[0x30], encoded.bytes.data, encoded.bytes.len);
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] as f32 == 42.777", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalStringScansUseCanonicalTextWithoutTerminators)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Memory_Copy(&backend.memory[0x20], "helloX", 6);
    Memory_Copy(&backend.memory[0x30], "line\nX", 6);

    Memmy_Value wide = {.type = Memmy_Type_WStr, .string = String8_Lit("h\xc3\xa9")};
    Memmy_EncodedValue encoded = {0};
    AssertEq(Memmy_Value_Encode(arena, &wide, &encoded, 0), Memmy_Status_Ok);
    Memory_Copy(&backend.memory[0x40], encoded.bytes.data, encoded.bytes.len - 2);

    Memmy_Value matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == \"hello\"", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1020);
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == \"line\\n\"", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1030);
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as wstr == \"h\xc3\xa9\"", &matches);
    Test_AssertAddressList(matches, 1);
    AssertEq(matches.list.addresses[0], 0x1040);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValueScanRhsFailsAndEvaluatesOnce)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x7a;
    backend.memory[0x80] = 0x7a;

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "[@0x1000..+0x40] as u8 == $missing", &expr);
    Memmy_Value value = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, 0), Memmy_Status_NotFound);
    AssertEq(backend.read_call_count, 0);

    Test_EvalExprText(env, arena, "[@0x1000..+0x40] as u8 == (@0x1080 as u8)", &value);
    Test_AssertAddressList(value, 1);
    AssertEq(value.list.addresses[0], 0x1020);
    AssertEq(backend.read_call_count, 2);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValueScansTraverseAccessibleIntersections)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0xab;
    backend.memory[0x50] = 0xab;
    Test_MemmyBackend_AddUnreadableRange(&backend, 0x1010, 0x1040);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as u8 == 0xab", &value);
    Test_AssertAddressList(value, 1);
    AssertEq(value.list.addresses[0], 0x1050);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "@0x1020 as u8", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, 0), Memmy_Status_Unreadable);

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

Test(Test_MemmyEvalIndexesAndTransformsEveryListFamily)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value value = {0};

    I64 signed_values[] = {-7, 42};
    value = Test_ListValue(arena, Memmy_Type_I16, ArrayCount(signed_values));
    value.list.signed_integers = signed_values;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("signed"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$signed[0]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_I16));
    AssertEq(value.signed_integer, -7);
    Test_EvalExprText(env, arena, "$signed => $", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_I16));
    AssertEq(value.list.signed_integers[1], 42);

    U64 unsigned_values[] = {3, U32_MAX};
    value = Test_ListValue(arena, Memmy_Type_U32, ArrayCount(unsigned_values));
    value.list.unsigned_integers = unsigned_values;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("unsigned"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$unsigned[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_U32));
    AssertEq(value.unsigned_integer, U32_MAX);
    Test_EvalExprText(env, arena, "$unsigned => $ as u64", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U64));
    AssertEq(value.list.unsigned_integers[1], U32_MAX);

    U32 f32_bits[] = {0x80000000u, 0x7fc01234u};
    value = Test_ListValue(arena, Memmy_Type_F32, ArrayCount(f32_bits));
    value.list.floating32_bits = f32_bits;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("f32s"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$f32s[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_F32));
    AssertEq(value.floating_bits, 0x7fc01234u);
    Test_EvalExprText(env, arena, "$f32s => $", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_F32));
    AssertEq(value.list.floating32_bits[0], 0x80000000u);
    AssertEq(value.list.floating32_bits[1], 0x7fc01234u);

    U64 f64_bits[] = {0x8000000000000000ull, 0x7ff8000000001234ull};
    value = Test_ListValue(arena, Memmy_Type_F64, ArrayCount(f64_bits));
    value.list.floating64_bits = f64_bits;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("f64s"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$f64s[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_F64));
    AssertEq(value.floating_bits, 0x7ff8000000001234ull);
    Test_EvalExprText(env, arena, "$f64s => $", &value);
    AssertEq(value.list.floating64_bits[0], 0x8000000000000000ull);

    Memmy_Addr addresses[] = {0x1010, 0x2020};
    value = Test_ListValue(arena, Memmy_Type_Address, ArrayCount(addresses));
    value.list.addresses = addresses;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("addresses"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$addresses[0]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Address));
    AssertEq(value.address, 0x1010);
    Test_EvalExprText(env, arena, "$addresses => $ + 4", &value);
    Test_AssertAddressList(value, 2);
    AssertEq(value.list.addresses[1], 0x2024);

    Memmy_Range ranges[] = {{.start = 0x10, .end = 0x20}, {.start = 0x30, .end = 0x40}};
    value = Test_ListValue(arena, Memmy_Type_Range, ArrayCount(ranges));
    value.list.ranges = ranges;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("ranges"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$ranges[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Range));
    AssertEq(value.range.start, 0x30);
    Test_EvalExprText(env, arena, "$ranges => $", &value);
    AssertEq(value.list.ranges[0].end, 0x20);

    U8 first[] = "alpha";
    U8 second[] = "beta";
    String8 strings[] = {String8_Make(first, 5), String8_Make(second, 4)};
    value = Test_ListValue(arena, Memmy_Type_Str, ArrayCount(strings));
    value.list.strings = strings;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("strings"), value), Memmy_Status_Ok);
    first[0] = 'X';
    second[0] = 'Y';
    Test_EvalExprText(env, arena, "$strings[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_Str));
    AssertStrEq(value.string, String8_Lit("beta"));
    Test_EvalExprText(env, arena, "$strings => $ as wstr", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_WStr));
    AssertStrEq(value.list.strings[0], String8_Lit("alpha"));
    AssertStrEq(value.list.strings[1], String8_Lit("beta"));

    String8 wide_strings[] = {String8_Lit("wide"), String8_Lit("text")};
    value = Test_ListValue(arena, Memmy_Type_WStr, ArrayCount(wide_strings));
    value.list.strings = wide_strings;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("wide_strings"), value), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$wide_strings[1]", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_WStr));
    AssertStrEq(value.string, String8_Lit("text"));
    Test_EvalExprText(env, arena, "$wide_strings => $", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_WStr));
    AssertStrEq(value.list.strings[0], String8_Lit("wide"));

    Test_EvalExprText(env, arena, "$f64s[0] |> $", &value);
    AssertTrue(Memmy_Type_Eq(value.type, Memmy_Type_F64));
    AssertEq(value.floating_bits, 0x8000000000000000ull);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTransformsPreserveResolvedTypesWhenEmptyOrFiltered)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Value empty = Test_ListValue(arena, Memmy_Type_I64, 0);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("empty"), empty), Memmy_Status_Ok);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$empty => $ as u16", &value);
    AssertTrue(Memmy_Type_IsList(value.type));
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U16));
    AssertEq(value.list.count, 0);

    Test_EvalExprText(env, arena, "$empty => @0x1000 as u8", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U8));
    AssertEq(value.list.count, 0);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "$empty => nil", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, 0), Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);

    arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x5a;
    Memmy_Addr addresses[] = {0x1020, 0x1050};
    Memmy_Value address_list = Test_ListValue(arena, Memmy_Type_Address, ArrayCount(addresses));
    address_list.list.addresses = addresses;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("addresses"), address_list), Memmy_Status_Ok);
    Test_MemmyBackend_AddUnreadableRange(&backend, 0x1040, 0x1060);

    Test_EvalExprText(env, arena, "$addresses => $ as u8", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U8));
    AssertEq(value.list.count, 1);
    AssertEq(value.list.unsigned_integers[0], 0x5a);

    Test_MemmyBackend_AddUnreadableRange(&backend, 0x1010, 0x1030);
    Test_EvalExprText(env, arena, "$addresses => $ as u8", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U8));
    AssertEq(value.list.count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTransformsFlattenAndRestoreNestedFlowBindings)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    I64 inputs[] = {10, 20};
    Memmy_Value input = Test_ListValue(arena, Memmy_Type_I64, ArrayCount(inputs));
    input.list.signed_integers = inputs;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("inputs"), input), Memmy_Status_Ok);

    U64 flattened[] = {7, 8};
    Memmy_Value flat = Test_ListValue(arena, Memmy_Type_U16, ArrayCount(flattened));
    flat.list.unsigned_integers = flattened;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("flat"), flat), Memmy_Status_Ok);

    Memmy_Value value = {0};
    Test_EvalExprText(env, arena, "$inputs => $flat", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_U16));
    AssertEq(value.list.count, 4);
    AssertEq(value.list.unsigned_integers[0], 7);
    AssertEq(value.list.unsigned_integers[1], 8);
    AssertEq(value.list.unsigned_integers[2], 7);
    AssertEq(value.list.unsigned_integers[3], 8);

    I64 inner_values[] = {1, 2};
    Memmy_Value inner = Test_ListValue(arena, Memmy_Type_I64, ArrayCount(inner_values));
    inner.list.signed_integers = inner_values;
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("inner"), inner), Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$inputs => (($inner => $ + 1) |> $[0]) + $", &value);
    AssertTrue(Memmy_Type_Eq(*value.type.list.element_type, Memmy_Type_I64));
    AssertEq(value.list.count, 2);
    AssertEq(value.list.signed_integers[0], 12);
    AssertEq(value.list.signed_integers[1], 22);

    Memmy_Type nested = {0};
    AssertEq(Memmy_Type_ListCreate(arena, flat.type, &nested, 0), Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_scan_transform = TestSuite_Make(
    "Memmy Eval Scan And Transform", TestCase_Make(Test_MemmyEvalPatternScanAssignmentMaterializesSemanticList),
    TestCase_Make(Test_MemmyEvalValueAndStringScansMaterializeSemanticLists),
    TestCase_Make(Test_MemmyEvalValueScansEvaluateAndConvertRhsExpressions),
    TestCase_Make(Test_MemmyEvalStringScansUseCanonicalTextWithoutTerminators),
    TestCase_Make(Test_MemmyEvalValueScanRhsFailsAndEvaluatesOnce),
    TestCase_Make(Test_MemmyEvalValueScansTraverseAccessibleIntersections),
    TestCase_Make(Test_MemmyEvalReferenceScansMaterializeSemanticLists),
    TestCase_Make(Test_MemmyEvalSemanticListsIndexTransformAndStore),
    TestCase_Make(Test_MemmyEvalNilShortCircuitsTransform),
    TestCase_Make(Test_MemmyEvalIndexesAndTransformsEveryListFamily),
    TestCase_Make(Test_MemmyEvalTransformsPreserveResolvedTypesWhenEmptyOrFiltered),
    TestCase_Make(Test_MemmyEvalTransformsFlattenAndRestoreNestedFlowBindings), );
