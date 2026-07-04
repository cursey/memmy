#include "test_memmy_common.h"

#include "memmy_exec.h"

static void Test_MemmyExecRange_Parse(Arena *arena, char *text, Memmy_RangeExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_MemmyExecRange_AddModule(Arena *arena, Memmy_ModuleList *modules)
{
    Memmy_Module *module = Memmy_ModuleList_Push(arena, modules);
    module->name = String8_Lit("client.dll");
    module->path = String8_Lit("C:\\game\\client.dll");
    module->base = 0x1000;
    module->size = 0x8000;
}

Test(Test_MemmyExecRangeResolvesModuleFullRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_MemmyExecRange_AddModule(arena, &modules);

    Memmy_RangeExpr expr = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>", &expr);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(0, &modules, &expr, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x9000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeResolvesModuleBracketAndSizedRanges)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_MemmyExecRange_AddModule(arena, &modules);

    Memmy_RangeExpr bracket = {0};
    Memmy_RangeExpr sized = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x1000..0x5000]", &bracket);
    Test_MemmyExecRange_Parse(arena, "<client.dll>[0x1000:+0x4000]", &sized);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(0, &modules, &bracket, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x2000);
    AssertEq(range.end, 0x6000);
    AssertEq(Memmy_RangeExpr_Resolve(0, &modules, &sized, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x2000);
    AssertEq(range.end, 0x6000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRangeResolvesModuleAddressSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_MemmyExecRange_AddModule(arena, &modules);

    Memmy_RangeExpr expr = {0};
    Test_MemmyExecRange_Parse(arena, "<client.dll>+0x123:+0x500", &expr);

    Memmy_Range range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Resolve(0, &modules, &expr, &range, &error), Memmy_Status_Ok);
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
    AssertEq(Memmy_RangeExpr_Resolve(0, 0, &expr, &range, &error), Memmy_Status_Ok);
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

TestSuite suite_memmy_exec_range =
    TestSuite_Make("Memmy Exec Range", TestCase_Make(Test_MemmyExecRangeResolvesModuleFullRange),
                   TestCase_Make(Test_MemmyExecRangeResolvesModuleBracketAndSizedRanges),
                   TestCase_Make(Test_MemmyExecRangeResolvesModuleAddressSizedRange),
                   TestCase_Make(Test_MemmyExecRangeResolvesAbsoluteAddressSizedRange),
                   TestCase_Make(Test_MemmyExecRangeRejectsAddressExprDotDotRanges), );
