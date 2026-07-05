#include "test_memmy_common.h"

#include "memmy_exec.h"

static void Test_MemmyExecRange_Parse(Arena *arena, char *text, Memmy_RangeExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_MemmyExecRange_AddModule(Test_MemmyBackend *backend)
{
    backend->module_count = 0;
    Test_MemmyBackend_AddModule(backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\game\\client.dll"), 0x1000,
                                0x8000);
}

static Memmy_Process Test_MemmyExecRange_Process(Test_MemmyBackend *backend)
{
    return (Memmy_Process){
        .backend = Test_MemmyBackend_AsBackend(backend),
        .pid = 4242,
        .pointer_width = Memmy_PointerWidth_64,
        .backend_data = backend,
    };
}

Test(Test_MemmyExecRangeResolvesModuleFullRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecRange_AddModule(&backend);
    Memmy_Process process = Test_MemmyExecRange_Process(&backend);

    Memmy_RangeExpr expr = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>", &expr);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(&process, &expr, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x9000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeResolvesModuleBracketAndSizedRanges)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecRange_AddModule(&backend);
    Memmy_Process process = Test_MemmyExecRange_Process(&backend);

    Memmy_RangeExpr bracket = {0};
    Memmy_RangeExpr sized = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x1000..0x5000]", &bracket);
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x1000:+0x4000]", &sized);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(&process, &bracket, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x2000);
    AssertEq(range.end, 0x6000);
    AssertEq(Memmy_RangeExpr_Resolve(&process, &sized, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x2000);
    AssertEq(range.end, 0x6000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeResolvesModuleAddressSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecRange_AddModule(&backend);
    Memmy_Process process = Test_MemmyExecRange_Process(&backend);

    Memmy_RangeExpr expr = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>+0x123:+0x500", &expr);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(&process, &expr, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1123);
    AssertEq(range.end, 0x1623);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeResolvesAbsoluteAddressSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr expr = {0};
    Test_MemmyExecRange_Parse(arena, "0x1000:+0x500", &expr);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(0, &expr, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1500);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeRejectsAddressExprDotDotRanges)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr expr = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_RangeExpr_Parse(arena, String8_Lit("<client.dll>+0x10..<client.dll>+0x20"), &expr, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("range"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeRejectsUnresolvedVariables)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecRange_AddModule(&backend);
    Memmy_Process process = Test_MemmyExecRange_Process(&backend);

    Memmy_RangeExpr variable_range = {0};
    Memmy_RangeExpr module_start = {0};
    Memmy_RangeExpr module_end = {0};
    Memmy_RangeExpr module_size = {0};
    Memmy_RangeExpr address_size = {0};
    Memmy_RangeExpr address_base = {0};
    Test_MemmyExecRange_Parse(arena, "$range", &variable_range);
    Test_MemmyExecRange_Parse(arena, "<client.dll>[$start..0x20]", &module_start);
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x10..$end]", &module_end);
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x10:+$size]", &module_size);
    Test_MemmyExecRange_Parse(arena, "0x1000:+$size", &address_size);
    Test_MemmyExecRange_Parse(arena, "$base:+0x20", &address_base);

    Memmy_RangeExpr *rejected[] = {
        &variable_range, &module_start, &module_end, &module_size, &address_size, &address_base,
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_Range range = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_RangeExpr_Resolve(&process, rejected[i], &range, &error), Memmy_Status_Unsupported);
        AssertEq(error.status, Memmy_Status_Unsupported);
        AssertTrue(String8_Eq(error.context, String8_Lit("range")) ||
                   String8_Eq(error.context, String8_Lit("address")));
    }

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_range =
    TestSuite_Make("Memmy Exec Range", TestCase_Make(Test_MemmyExecRangeResolvesModuleFullRange),
                   TestCase_Make(Test_MemmyExecRangeResolvesModuleBracketAndSizedRanges),
                   TestCase_Make(Test_MemmyExecRangeResolvesModuleAddressSizedRange),
                   TestCase_Make(Test_MemmyExecRangeResolvesAbsoluteAddressSizedRange),
                   TestCase_Make(Test_MemmyExecRangeRejectsAddressExprDotDotRanges),
                   TestCase_Make(Test_MemmyExecRangeRejectsUnresolvedVariables), );
