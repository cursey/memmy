#include "test_memmy_eval_common.h"

Test(Test_MemmyEvalPatternScanAssignmentMaterializesAddressList)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40]{10 11}");
    MemmyEval_Value matches = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("matches"), &matches), Memmy_Status_Ok);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
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
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x30] = 0x10;
    backend.memory[0x31] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40] as u16 == 0x1110");
    MemmyEval_Value matches = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("matches"), &matches), Memmy_Status_Ok);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
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
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    U8 hello[] = {'h', 'e', 'l', 'l', 'o'};
    U8 quoted_hello[] = {'"', 'h', 'e', 'l', 'l', 'o', '"'};
    U8 wide_hi[] = {'H', 0, 'i', 0};
    Memory_Copy(&backend.memory[0x20], hello, ArrayCount(hello));
    Memory_Copy(&backend.memory[0x30], quoted_hello, ArrayCount(quoted_hello));
    Memory_Copy(&backend.memory[0x40], wide_hi, ArrayCount(wide_hi));

    MemmyEval_Value str_matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as str == \"hello\"", &str_matches);
    AssertEq(str_matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(str_matches.address_count, 2);
    AssertEq(str_matches.addresses[0], 0x1020);
    AssertEq(str_matches.addresses[1], 0x1031);

    MemmyEval_Value wstr_matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x80] as wstr == \"Hi\"", &wstr_matches);
    AssertEq(wstr_matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(wstr_matches.address_count, 1);
    AssertEq(wstr_matches.addresses[0], 0x1040);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalReferenceScansMaterializeAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU32LE(backend.memory + 0x20, 0x1040 - 0x1020 - 4);
    Test_EvalWriteU32LE(backend.memory + 0x30, (U32)(I32)(0x1040 - 0x1030 - 4));

    MemmyEval_Value ptr = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs ptr @0x1040", &ptr);
    AssertEq(ptr.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(ptr.address_count, 1);
    AssertEq(ptr.addresses[0], 0x1010);

    MemmyEval_Value rel32 = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs rel32 @0x1040", &rel32);
    AssertEq(rel32.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(rel32.address_count, 2);
    AssertEq(rel32.addresses[0], 0x1020);
    AssertEq(rel32.addresses[1], 0x1030);

    MemmyEval_Value any = {0};
    Test_EvalExprText(env, arena, "[@0x1000..+0x40] refs any @0x1040", &any);
    AssertEq(any.kind, MemmyEval_ValueKind_AddressList);
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
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalWriteU64LE(backend.memory + 0x10, 0x1040);
    Test_EvalWriteU64LE(backend.memory + 0x20, 0x1040);

    Test_EvalStatementText(env, arena, "$target = @0x1040");
    Test_EvalStatementText(env, arena, "$refs = [0..] refs ptr $target");
    MemmyEval_Value refs = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("refs"), &refs), Memmy_Status_Ok);
    AssertEq(refs.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(refs.address_count, 2);
    AssertEq(refs.addresses[0], 0x1010);
    AssertEq(refs.addresses[1], 0x1020);

    MemmyEval_Value second = {0};
    Test_EvalExprText(env, arena, "$refs[1]", &second);
    AssertEq(second.kind, MemmyEval_ValueKind_Address);
    AssertEq(second.address, 0x1020);

    MemmyEval_Value transformed = {0};
    Test_EvalExprText(env, arena, "[0..] refs ptr @0x1040 => $ + 4", &transformed);
    AssertEq(transformed.kind, MemmyEval_ValueKind_AddressList);
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
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x10;
    backend.memory[0x21] = 0x11;

    Test_EvalStatementText(env, arena, "$matches = [@0x1000..+0x40]{10 11}");
    MemmyEval_Value second = {0};
    Test_EvalExprText(env, arena, "$matches[1]", &second);
    AssertEq(second.kind, MemmyEval_ValueKind_Address);
    AssertEq(second.address, 0x1020);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "$matches[2]", &expr);
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &second, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("index"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalIndexesValueScanExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x30] = 0x10;
    backend.memory[0x31] = 0x11;

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "([@0x1000..+0x40] as u16 == 0x1110)[1]", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1030);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalListTransformsAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Addr refs[] = {0x1000, 0x2000};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    MemmyEval_Value addresses = {0};
    Test_EvalExprText(env, arena, "$refs => $ + 4", &addresses);
    AssertEq(addresses.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(addresses.address_count, 2);
    AssertEq(addresses.addresses[0], 0x1004);
    AssertEq(addresses.addresses[1], 0x2004);

    MemmyEval_Value ranges = {0};
    Test_EvalStatementResult(env, arena, "$ranges = $refs => [$..+0x20]", &ranges);
    AssertEq(ranges.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(ranges.range_count, 2);
    AssertEq(ranges.ranges[0].start, 0x1000);
    AssertEq(ranges.ranges[0].end, 0x1020);
    AssertEq(ranges.ranges[1].start, 0x2000);
    AssertEq(ranges.ranges[1].end, 0x2020);

    MemmyEval_Value offset_ranges = {0};
    Test_EvalStatementResult(env, arena, "$offset_ranges = $refs => [$ - 0x80..+0x180]", &offset_ranges);
    AssertEq(offset_ranges.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(offset_ranges.range_count, 2);
    AssertEq(offset_ranges.ranges[0].start, 0x0f80);
    AssertEq(offset_ranges.ranges[0].end, 0x1100);
    AssertEq(offset_ranges.ranges[1].start, 0x1f80);
    AssertEq(offset_ranges.ranges[1].end, 0x2100);

    MemmyEval_Value second = {0};
    Test_EvalExprText(env, arena, "$ranges[1]", &second);
    AssertEq(second.kind, MemmyEval_ValueKind_Range);
    AssertEq(second.range.start, 0x2000);
    AssertEq(second.range.end, 0x2020);

    MemmyEval_Value stored = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("ranges"), &stored), Memmy_Status_Ok);
    AssertEq(stored.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(stored.range_count, 2);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalListTransformsRangeLists)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Range ranges[] = {
        {.start = 0x1000, .end = 0x1010},
        {.start = 0x2000, .end = 0x2010},
    };
    Memmy_Addr refs[] = {0x10, 0x20};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("ranges"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_RangeList,
                                                 .ranges = ranges,
                                                 .range_count = ArrayCount(ranges)}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    MemmyEval_Value addresses = {0};
    Test_EvalExprText(env, arena, "$ranges => $ + 4", &addresses);
    AssertEq(addresses.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(addresses.address_count, 2);
    AssertEq(addresses.addresses[0], 0x1004);
    AssertEq(addresses.addresses[1], 0x2004);

    MemmyEval_Value flattened = {0};
    Test_EvalExprText(env, arena, "$ranges => $refs", &flattened);
    AssertEq(flattened.kind, MemmyEval_ValueKind_AddressList);
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
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Addr refs[] = {U64_MAX};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    MemmyAst_Node *expr = 0;
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};
    Test_EvalParseExpr(arena, "@0x1000 => $ + 4", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("transform"));

    Test_EvalParseExpr(arena, "$refs => 42", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("transform"));

    Memmy_Addr *empty_refs = 0;
    AssertEq(
        MemmyEval_Env_Set(
            env, String8_Lit("empty_refs"),
            (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList, .addresses = empty_refs, .address_count = 0}),
        Memmy_Status_Ok);
    Memmy_Range *empty_ranges = 0;
    AssertEq(MemmyEval_Env_Set(
                 env, String8_Lit("empty_ranges"),
                 (MemmyEval_Value){.kind = MemmyEval_ValueKind_RangeList, .ranges = empty_ranges, .range_count = 0}),
             Memmy_Status_Ok);

    Test_EvalParseExpr(arena, "$empty_refs => $ + 4", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    Test_EvalParseExpr(arena, "$empty_ranges => $ + 4", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    Test_EvalParseExpr(arena, "$refs => $ + 1", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    Memmy_Addr small_refs[] = {0x1000};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = small_refs,
                                                 .address_count = ArrayCount(small_refs)}),
             Memmy_Status_Ok);
    Test_EvalParseExpr(arena, "($refs => [$..+0x10])[1]", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("index"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalEmptyListTransformDoesNotEvaluateRhsEffects)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Memmy_Addr *empty_refs = 0;
    AssertEq(
        MemmyEval_Env_Set(
            env, String8_Lit("empty_refs"),
            (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList, .addresses = empty_refs, .address_count = 0}),
        Memmy_Status_Ok);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "$empty_refs => (@0x1004 as u8 = 1)", &expr);
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);
    AssertEq(backend.memory[4], 4);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalNilLiteralAndComposition)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "nil", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);
    Test_EvalStatementText(env, arena, "$x = nil");
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("x"), &value), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);
    Test_EvalExprText(env, arena, "nil |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);
    Test_EvalExprText(env, arena, "nil => (@0x1004 as u8 = 1)", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);
    AssertEq(backend.memory[4], 4);

    MemmyAst_Node *expr = 0;
    Memmy_Error error = {0};
    Test_EvalParseExpr(arena, "nil[0]", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("index"));
    AssertStrEq(error.message, String8_Lit("list index out of range"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalListTransformFiltersFailuresAndNil)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Memmy_Addr refs[] = {0x1000, U64_MAX, 0x2000};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "$refs => $ + 1", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(value.address_count, 2);
    AssertEq(value.addresses[0], 0x1001);
    AssertEq(value.addresses[1], 0x2001);

    Memmy_Addr range_starts[] = {0x1000, 0x2000, 0x1100};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("range_starts"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = range_starts,
                                                 .address_count = ArrayCount(range_starts)}),
             Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$range_starts => [$..@0x1500]", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(value.range_count, 2);
    AssertEq(value.ranges[0].start, 0x1000);
    AssertEq(value.ranges[1].start, 0x1100);

    Test_EvalExprText(env, arena, "$refs => nil", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    AssertEq(MemmyEval_Env_Set(env, String8_Lit("empty"), (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList}),
             Memmy_Status_Ok);
    Test_EvalExprText(env, arena, "$refs => $empty", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    Test_MemmyBackend_SetReadStatus(&backend, Memmy_Status_AccessDenied);
    Test_EvalExprText(env, arena, "$refs => $ as u8", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Nil);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAnchorTargetExampleFlow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0xaa;
    backend.memory[0x21] = 0xbb;
    backend.memory[0x24] = 0xef;
    backend.memory[0x25] = 0xbe;

    Test_EvalStatementText(env, arena, "$anchor = [@0x1000..+0x40]{aa bb}[0]");
    Test_EvalStatementText(env, arena, "$target = $anchor + 4");

    MemmyEval_Value target = {0};
    Test_EvalExprText(env, arena, "$target", &target);
    AssertEq(target.kind, MemmyEval_ValueKind_Address);
    AssertEq(target.address, 0x1024);

    MemmyEval_Value target_value = {0};
    Test_EvalExprText(env, arena, "$target as u16", &target_value);
    AssertEq(target_value.kind, MemmyEval_ValueKind_TypedValue);
    AssertEq(target_value.constant, 0xbeef);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValuePipesPreserveValues)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    Memmy_Addr refs[] = {0x1000, 0x2000};
    Memmy_Range ranges[] = {{.start = 0x3000, .end = 0x3020}};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("refs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = refs,
                                                 .address_count = ArrayCount(refs)}),
             Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("ranges"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_RangeList,
                                                 .ranges = ranges,
                                                 .range_count = ArrayCount(ranges)}),
             Memmy_Status_Ok);
    AssertEq(
        MemmyEval_Env_Set(env, String8_Lit("empty_refs"), (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList}),
        Memmy_Status_Ok);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("max"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_Address, .address = U64_MAX}),
             Memmy_Status_Ok);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "42 |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Const);
    AssertEq(value.constant, 42);
    Test_EvalExprText(env, arena, "@0x1234 |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1234);
    Test_EvalExprText(env, arena, "[@0x1234..+0x20] |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Range);
    AssertEq(value.range.start, 0x1234);
    AssertEq(value.range.end, 0x1254);
    Test_EvalExprText(env, arena, "$refs |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(value.address_count, 2);
    AssertEq(value.addresses[1], 0x2000);
    Test_EvalExprText(env, arena, "$ranges |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(value.range_count, 1);
    Test_EvalExprText(env, arena, "$empty_refs |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(value.address_count, 0);
    Test_EvalExprText(env, arena, "$refs |> 7", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Const);
    AssertEq(value.constant, 7);
    Test_EvalExprText(env, arena, "$refs => $ + 4 |> $[0]", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1004);
    Test_EvalExprText(env, arena, "$refs => ((7 |> $) + $)", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(value.addresses[0], 0x1007);
    AssertEq(value.addresses[1], 0x2007);

    MemmyAst_Node *expr = 0;
    Memmy_Error error = {0};
    Test_EvalParseExpr(arena, "1 |> $max + 1", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalValuePipesTypedValuesAndScanLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    backend.memory[0x20] = 0x2a;
    backend.memory[0x30] = 0x90;
    Test_EvalWriteU64LE(backend.memory + 0x40, 0x1080);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "@0x1020 as u8 |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_TypedValue);
    AssertEq(value.constant, 0x2a);
    Test_EvalExprText(env, arena, "[@0x1000..+0x40]{90} |> $", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(value.address_count, 1);
    AssertEq(value.addresses[0], 0x1030);
    U64 read_call_count = backend.read_call_count;
    Test_EvalExprText(env, arena, "@0x1020 as u8 |> @0x1030 as u8", &value);
    AssertEq(backend.read_call_count - read_call_count, 2);
    Test_EvalExprText(env, arena, "(@0x1040 |> $)->", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1080);
    Test_EvalExprText(env, arena, "([@0x1040..+0x8] |> $)->", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1080);

    MemmyAst_Node *expr = 0;
    Test_EvalParseExpr(arena, "(1 |> $)->", &expr);
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("address"));

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
    TestCase_Make(Test_MemmyEvalNilLiteralAndComposition),
    TestCase_Make(Test_MemmyEvalListTransformFiltersFailuresAndNil),
    TestCase_Make(Test_MemmyEvalAnchorTargetExampleFlow), TestCase_Make(Test_MemmyEvalValuePipesPreserveValues),
    TestCase_Make(Test_MemmyEvalValuePipesTypedValuesAndScanLists), );
