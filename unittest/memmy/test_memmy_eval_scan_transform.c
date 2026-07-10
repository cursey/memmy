#include "test_memmy_eval_common.h"

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

Test(Test_MemmyEvalQuotedStringValueScansDecodeDslLiterals)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    U8 hello[] = {'h', 'e', 'l', 'l', 'o'};
    U8 quoted_hello[] = {'"', 'h', 'e', 'l', 'l', 'o', '"'};
    U8 wide_hi[] = {'H', 0, 'i', 0};
    Memory_Copy(&backend.memory[0x20], hello, ArrayCount(hello));
    Memory_Copy(&backend.memory[0x30], quoted_hello, ArrayCount(quoted_hello));
    Memory_Copy(&backend.memory[0x40], wide_hi, ArrayCount(wide_hi));

    Memmy_EvalValue str_matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == \"hello\"", &str_matches);
    AssertEq(str_matches.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(str_matches.address_count, 2);
    AssertEq(str_matches.addresses[0], 0x1020);
    AssertEq(str_matches.addresses[1], 0x1031);

    Memmy_EvalValue wstr_matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as wstr == \"Hi\"", &wstr_matches);
    AssertEq(wstr_matches.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(wstr_matches.address_count, 1);
    AssertEq(wstr_matches.addresses[0], 0x1040);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalReferenceScansMaterializeAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU32LE(backend.memory + 0x20, 0x1040 - 0x1020 - 4);
    Test_EvalWriteU32LE(backend.memory + 0x30, (U32)(I32)(0x1040 - 0x1030 - 4));

    Memmy_EvalValue ptr = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs ptr @0x1040", &ptr);
    AssertEq(ptr.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(ptr.address_count, 1);
    AssertEq(ptr.addresses[0], 0x1010);

    Memmy_EvalValue rel32 = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs rel32 @0x1040", &rel32);
    AssertEq(rel32.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(rel32.address_count, 2);
    AssertEq(rel32.addresses[0], 0x1020);
    AssertEq(rel32.addresses[1], 0x1030);

    Memmy_EvalValue any = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs any @0x1040", &any);
    AssertEq(any.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(any.address_count, 3);
    AssertEq(any.addresses[0], 0x1010);
    AssertEq(any.addresses[1], 0x1020);
    AssertEq(any.addresses[2], 0x1030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalReferenceScanAssignmentsIndexesTransformsAndProcessRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU64LE(backend.memory + 0x20, 0x1040);

    Test_EvalStatementText(env, arena, "$target = @0x1040");
    Test_EvalStatementText(env, arena, "$refs = [0..] refs ptr $target");
    Memmy_EvalValue refs = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("refs"), &refs), Memmy_Status_Ok);
    AssertEq(refs.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(refs.address_count, 2);
    AssertEq(refs.addresses[0], 0x1010);
    AssertEq(refs.addresses[1], 0x1020);

    Memmy_EvalValue second = {0};
    Test_EvalExprText(env, arena, "$refs[1]", &second);
    AssertEq(second.kind, Memmy_EvalValueKind_Address);
    AssertEq(second.address, 0x1020);

    Memmy_EvalValue transformed = {0};
    Test_EvalExprText(env, arena, "[0..] refs ptr @0x1040 => $ + 4", &transformed);
    AssertEq(transformed.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(transformed.address_count, 2);
    AssertEq(transformed.addresses[0], 0x1014);
    AssertEq(transformed.addresses[1], 0x1024);

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

Test(Test_MemmyEvalListTransformsAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_Addr refs[] = {0x1000, 0x2000};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("refs"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    Memmy_EvalValue addresses = {0};
    Test_EvalExprText(env, arena, "$refs => $ + 4", &addresses);
    AssertEq(addresses.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(addresses.address_count, 2);
    AssertEq(addresses.addresses[0], 0x1004);
    AssertEq(addresses.addresses[1], 0x2004);

    Memmy_EvalValue ranges = {0};
    Test_EvalStatementResult(env, arena, "$ranges = $refs => [$..+0x20]", &ranges);
    AssertEq(ranges.kind, Memmy_EvalValueKind_RangeList);
    AssertEq(ranges.range_count, 2);
    AssertEq(ranges.ranges[0].start, 0x1000);
    AssertEq(ranges.ranges[0].end, 0x1020);
    AssertEq(ranges.ranges[1].start, 0x2000);
    AssertEq(ranges.ranges[1].end, 0x2020);

    Memmy_EvalValue offset_ranges = {0};
    Test_EvalStatementResult(env, arena, "$offset_ranges = $refs => [$ - 0x80..+0x180]", &offset_ranges);
    AssertEq(offset_ranges.kind, Memmy_EvalValueKind_RangeList);
    AssertEq(offset_ranges.range_count, 2);
    AssertEq(offset_ranges.ranges[0].start, 0x0f80);
    AssertEq(offset_ranges.ranges[0].end, 0x1100);
    AssertEq(offset_ranges.ranges[1].start, 0x1f80);
    AssertEq(offset_ranges.ranges[1].end, 0x2100);

    Memmy_EvalValue second = {0};
    Test_EvalExprText(env, arena, "$ranges[1]", &second);
    AssertEq(second.kind, Memmy_EvalValueKind_Range);
    AssertEq(second.range.start, 0x2000);
    AssertEq(second.range.end, 0x2020);

    Memmy_EvalValue stored = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("ranges"), &stored), Memmy_Status_Ok);
    AssertEq(stored.kind, Memmy_EvalValueKind_RangeList);
    AssertEq(stored.range_count, 2);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalListTransformsRangeLists)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_Range ranges[] = {
        {.start = 0x1000, .end = 0x1010},
        {.start = 0x2000, .end = 0x2010},
    };
    Memmy_Addr refs[] = {0x10, 0x20};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("ranges"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_RangeList,
                                                 .ranges = ranges,
                                                 .range_count = ArrayCount(ranges)}),
             Memmy_Status_Ok);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("refs"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    Memmy_EvalValue addresses = {0};
    Test_EvalExprText(env, arena, "$ranges => $ + 4", &addresses);
    AssertEq(addresses.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(addresses.address_count, 2);
    AssertEq(addresses.addresses[0], 0x1004);
    AssertEq(addresses.addresses[1], 0x2004);

    Memmy_EvalValue flattened = {0};
    Test_EvalExprText(env, arena, "$ranges => $refs", &flattened);
    AssertEq(flattened.kind, Memmy_EvalValueKind_AddressList);
    AssertEq(flattened.address_count, 4);
    AssertEq(flattened.addresses[0], 0x10);
    AssertEq(flattened.addresses[1], 0x20);
    AssertEq(flattened.addresses[2], 0x10);
    AssertEq(flattened.addresses[3], 0x20);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalListTransformErrors)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_Addr refs[] = {U64_MAX};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("refs"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    Memmy_AstNode *expr = 0;
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};
    Test_EvalParseExpr(arena, "@0x1000 => $ + 4", &expr);
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("transform"));

    Test_EvalParseExpr(arena, "$refs => 42", &expr);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("transform"));

    Memmy_Addr *empty_refs = 0;
    AssertEq(
        Memmy_EvalEnv_Set(
            env, String8_Lit("empty_refs"),
            (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList, .addresses = empty_refs, .address_count = 0}),
        Memmy_Status_Ok);
    Memmy_Range *empty_ranges = 0;
    AssertEq(Memmy_EvalEnv_Set(
                 env, String8_Lit("empty_ranges"),
                 (Memmy_EvalValue){.kind = Memmy_EvalValueKind_RangeList, .ranges = empty_ranges, .range_count = 0}),
             Memmy_Status_Ok);

    Test_EvalParseExpr(arena, "$empty_refs => $ + 4", &expr);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("transform"));
    AssertStrEq(error.message, String8_Lit("transform input list is empty"));

    Test_EvalParseExpr(arena, "$empty_ranges => $ + 4", &expr);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("transform"));
    AssertStrEq(error.message, String8_Lit("transform input list is empty"));

    Test_EvalParseExpr(arena, "$refs => $ + 1", &expr);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    Memmy_Addr small_refs[] = {0x1000};
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("refs"),
                               (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList,
                                                 .addresses = small_refs,
                                                 .address_count = ArrayCount(small_refs)}),
             Memmy_Status_Ok);
    Test_EvalParseExpr(arena, "($refs => [$..+0x10])[1]", &expr);
    error = (Memmy_Error){0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("index"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalEmptyListTransformDoesNotEvaluateRhsEffects)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Memmy_Addr *empty_refs = 0;
    AssertEq(
        Memmy_EvalEnv_Set(
            env, String8_Lit("empty_refs"),
            (Memmy_EvalValue){.kind = Memmy_EvalValueKind_AddressList, .addresses = empty_refs, .address_count = 0}),
        Memmy_Status_Ok);

    Memmy_AstNode *expr = 0;
    Test_EvalParseExpr(arena, "$empty_refs => (@0x1004 as u8 = 1)", &expr);
    Memmy_EvalValue value = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_EvalExpr(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("transform"));
    AssertStrEq(error.message, String8_Lit("transform input list is empty"));
    AssertEq(backend.memory[4], 4);

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

TestSuite suite_memmy_eval_scan_transform = TestSuite_Make(
    "Memmy Eval Scan And Transform", TestCase_Make(Test_MemmyEvalPatternScanAssignmentMaterializesAddressList),
    TestCase_Make(Test_MemmyEvalValueScanAssignmentMaterializesAddressList),
    TestCase_Make(Test_MemmyEvalQuotedStringValueScansDecodeDslLiterals),
    TestCase_Make(Test_MemmyEvalReferenceScansMaterializeAddressLists),
    TestCase_Make(Test_MemmyEvalReferenceScanAssignmentsIndexesTransformsAndProcessRange),
    TestCase_Make(Test_MemmyEvalIndexesAssignedAddressLists), TestCase_Make(Test_MemmyEvalIndexesValueScanExpressions),
    TestCase_Make(Test_MemmyEvalListTransformsAddressLists), TestCase_Make(Test_MemmyEvalListTransformsRangeLists),
    TestCase_Make(Test_MemmyEvalListTransformErrors),
    TestCase_Make(Test_MemmyEvalEmptyListTransformDoesNotEvaluateRhsEffects),
    TestCase_Make(Test_MemmyEvalAnchorTargetExampleFlow), );
