#include "memmy_dsl.h"
#include "test_framework.h"

static void Test_ParseRangeExpr(Arena *arena, char *text, Memmy_RangeExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

Test(Test_MemmyExprRangeParsesTargetRefs)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr module = {0};
    Memmy_RangeExpr whole_process = {0};

    Test_ParseRangeExpr(arena, "<client.dll>", &module);
    Test_ParseRangeExpr(arena, "<game.exe!>", &whole_process);

    AssertEq(module.kind, Memmy_RangeExprKind_Target);
    AssertEq(module.target.kind, Memmy_TargetExprKind_Module);
    AssertStrEq(module.target.module_name, String8_Lit("client.dll"));

    AssertEq(whole_process.kind, Memmy_RangeExprKind_Target);
    AssertEq(whole_process.target.kind, Memmy_TargetExprKind_WholeProcess);
    AssertEq(whole_process.target.process.kind, Memmy_ProcessSelectorKind_Name);
    AssertStrEq(whole_process.target.process.name, String8_Lit("game.exe"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeParsesModuleBracketRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Test_ParseRangeExpr(arena, "<game.exe!client.dll>[0x1000..0x5000]", &range);

    AssertEq(range.kind, Memmy_RangeExprKind_ModuleOffset);
    AssertEq(range.target.kind, Memmy_TargetExprKind_Module);
    AssertStrEq(range.target.process.name, String8_Lit("game.exe"));
    AssertStrEq(range.target.module_name, String8_Lit("client.dll"));
    AssertEq(range.start_offset, 0x1000);
    AssertEq(range.end_offset, 0x5000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeParsesModuleSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Test_ParseRangeExpr(arena, "<client.dll>[0x1000:+0x4000]", &range);

    AssertEq(range.kind, Memmy_RangeExprKind_ModuleSized);
    AssertEq(range.target.kind, Memmy_TargetExprKind_Module);
    AssertStrEq(range.target.module_name, String8_Lit("client.dll"));
    AssertEq(range.start_offset, 0x1000);
    AssertEq(range.size, 0x4000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeParsesModuleSizedRangeConstSize)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Test_ParseRangeExpr(arena, "<client.dll>[0x1000:+0x2000*2]", &range);

    AssertEq(range.kind, Memmy_RangeExprKind_ModuleSized);
    AssertEq(range.start_offset, 0x1000);
    AssertEq(range.size, 0x4000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeParsesAddressSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr absolute = {0};
    Memmy_RangeExpr module = {0};

    Test_ParseRangeExpr(arena, "0x1000:+0x20", &absolute);
    Test_ParseRangeExpr(arena, "<client.dll>+0x123:+0x500", &module);

    AssertEq(absolute.kind, Memmy_RangeExprKind_AddressSized);
    AssertEq(absolute.address.base_kind, Memmy_AddressExprBaseKind_Absolute);
    AssertEq(absolute.address.absolute, 0x1000);
    AssertEq(absolute.size, 0x20);

    AssertEq(module.kind, Memmy_RangeExprKind_AddressSized);
    AssertEq(module.address.base_kind, Memmy_AddressExprBaseKind_Target);
    AssertEq(module.address.ops.count, 1);
    AssertEq(module.size, 0x500);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeRejectsAddressExprDotDotRanges)
{
    struct
    {
        String8 text;
        U64 byte_offset;
    } rejected[] = {
        {String8_Lit("0x100..0x200"), 5},
        {String8_Lit("<client.dll>+0x100..0x200"), 18},
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_RangeExpr range = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_RangeExpr_Parse(arena, rejected[i].text, &range, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("range"));
        AssertEq(error.byte_offset, rejected[i].byte_offset);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprRangeRejectsWholeProcessBracketRanges)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_Lit("<game.exe!>[0x1000:+0x20]"), &range, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("range"));
    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeRejectsNegativeModuleSizedRangeSize)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_Lit("<client.dll>[0x1000:+-1]"), &range, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("range"));
    Arena_Destroy(arena);
}

Test(Test_MemmyExprRangeReportsByteOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_RangeExpr range = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_RangeExpr_Parse(arena, String8_Lit("<client.dll>[0x1000:+0x]"), &range, &error),
             Memmy_Status_ParseError);
    AssertEq(error.byte_offset, 23);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_dsl_range = TestSuite_Make(
    "Memmy DSL Range", TestCase_Make(Test_MemmyExprRangeParsesTargetRefs),
    TestCase_Make(Test_MemmyExprRangeParsesModuleBracketRange),
    TestCase_Make(Test_MemmyExprRangeParsesModuleSizedRange), TestCase_Make(Test_MemmyExprRangeParsesAddressSizedRange),
    TestCase_Make(Test_MemmyExprRangeParsesModuleSizedRangeConstSize),
    TestCase_Make(Test_MemmyExprRangeRejectsAddressExprDotDotRanges),
    TestCase_Make(Test_MemmyExprRangeRejectsWholeProcessBracketRanges),
    TestCase_Make(Test_MemmyExprRangeRejectsNegativeModuleSizedRangeSize),
    TestCase_Make(Test_MemmyExprRangeReportsByteOffsets), );
