#include "memmy_dsl.h"
#include "test_framework.h"

static void Test_ParseMemoryExpr(Arena *arena, char *text, Memmy_MemoryExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

Test(Test_MemmyExprMemoryDispatchesRequiredExamples)
{
    struct
    {
        char *text;
        Memmy_MemoryExprKind kind;
    } cases[] = {
        {"0x000001d856780004 : u32", Memmy_MemoryExprKind_Peek},
        {"<client.dll>+0x123", Memmy_MemoryExprKind_Address},
        {"<client.dll>+0x123->0x8", Memmy_MemoryExprKind_Address},
        {"<game.exe!client.dll>+0x123 : i32", Memmy_MemoryExprKind_Peek},
        {"<game.exe!client.dll>+0x123 : i32 = 77", Memmy_MemoryExprKind_Poke},
        {"<game.exe!>0x1234", Memmy_MemoryExprKind_Address},
        {"<game.exe!>0x1234 : u32", Memmy_MemoryExprKind_Peek},
        {"<game.exe!>0x1234 : u32 = 42", Memmy_MemoryExprKind_Poke},
        {"<game.exe!client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}", Memmy_MemoryExprKind_PatternScan},
        {"<game.exe!> : i32 == 42", Memmy_MemoryExprKind_ValueScan},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_MemoryExpr expr = {0};
        Test_ParseMemoryExpr(arena, cases[i].text, &expr);
        AssertEq(expr.kind, cases[i].kind);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprMemoryDispatchesEachTopLevelKind)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr address = {0};
    Memmy_MemoryExpr peek = {0};
    Memmy_MemoryExpr poke = {0};
    Memmy_MemoryExpr pattern = {0};
    Memmy_MemoryExpr value = {0};

    Test_ParseMemoryExpr(arena, "0x1000", &address);
    Test_ParseMemoryExpr(arena, "0x1000 : u32", &peek);
    Test_ParseMemoryExpr(arena, "0x1000 : u32 = 77", &poke);
    Test_ParseMemoryExpr(arena, "<client.dll>{48 8b ??}", &pattern);
    Test_ParseMemoryExpr(arena, "<game.exe!> : i32 == 42", &value);

    AssertEq(address.kind, Memmy_MemoryExprKind_Address);
    AssertEq(peek.kind, Memmy_MemoryExprKind_Peek);
    AssertEq(poke.kind, Memmy_MemoryExprKind_Poke);
    AssertEq(pattern.kind, Memmy_MemoryExprKind_PatternScan);
    AssertEq(value.kind, Memmy_MemoryExprKind_ValueScan);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryParsesPatternScansWithWildcards)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemoryExpr(arena, "<client.dll>[0x1000:+0x4000]{48 8b ?? ?? 89}", &expr);

    AssertEq(expr.kind, Memmy_MemoryExprKind_PatternScan);
    AssertEq(expr.range.kind, Memmy_RangeExprKind_TargetSized);
    AssertEq(expr.pattern.count, 5);
    AssertEq(expr.pattern.bytes[0].value, 0x48);
    AssertEq(expr.pattern.bytes[1].value, 0x8b);
    AssertTrue(expr.pattern.bytes[2].wildcard);
    AssertTrue(expr.pattern.bytes[3].wildcard);
    AssertEq(expr.pattern.bytes[4].value, 0x89);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryParsesPatternScanWithModuleConstSize)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemoryExpr(arena, "<client.dll>[0x1000:+0x2000*2]{90}", &expr);

    AssertEq(expr.kind, Memmy_MemoryExprKind_PatternScan);
    AssertEq(expr.range.kind, Memmy_RangeExprKind_TargetSized);
    AssertEq(expr.range.size, 0x4000);
    AssertEq(expr.pattern.count, 1);
    AssertEq(expr.pattern.bytes[0].value, 0x90);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryParsesExactValueScans)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemoryExpr(arena, "<game.exe!> : i32 == 42", &expr);

    AssertEq(expr.kind, Memmy_MemoryExprKind_ValueScan);
    AssertEq(expr.range.kind, Memmy_RangeExprKind_Target);
    AssertEq(expr.range.target.kind, Memmy_TargetExprKind_WholeProcess);
    AssertEq(expr.type.kind, Memmy_TypeKind_I32);
    AssertStrEq(expr.value_text, String8_Lit("42"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryRejectsOrderingComparisons)
{
    String8 rejected[] = {
        String8_Lit("<game.exe!> : i32 > 42"),
        String8_Lit("<game.exe!> : i32 >= 42"),
        String8_Lit("<game.exe!> : i32 < 42"),
        String8_Lit("<game.exe!> : i32 <= 42"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_MemoryExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_MemoryExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprMemoryRejectsBareWholeProcessTargets)
{
    String8 rejected[] = {
        String8_Lit("<game.exe!>"),
        String8_Lit("<123!>"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_MemoryExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_MemoryExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        AssertStrEq(error.input, rejected[i]);
        AssertStrEq(error.message, String8_Lit("whole-process target is not a valid address base"));
        AssertEq(error.byte_offset, 0);
        AssertEq(error.byte_count, rejected[i].len);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprMemoryParsesProcessQualifiedAddressSizedPatternScan)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemoryExpr(arena, "<game.exe!>0x1234:+0x100 { 90 }", &expr);

    AssertEq(expr.kind, Memmy_MemoryExprKind_PatternScan);
    AssertEq(expr.range.kind, Memmy_RangeExprKind_AddressSized);
    AssertEq(expr.range.address.base_kind, Memmy_AddressExprBaseKind_ProcessAbsolute);
    AssertEq(expr.range.address.absolute, 0x1234);
    AssertEq(expr.range.size, 0x100);
    AssertEq(expr.pattern.count, 1);
    AssertEq(expr.pattern.bytes[0].value, 0x90);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryRejectsRhsAddressExpressionsForPokes)
{
    String8 rejected[] = {
        String8_Lit("0x1000 : u32 = <client.dll>+0x4"),
        String8_Lit("0x1000 : u32 = <game.exe!>0x2000"),
        String8_Lit("0x1000 : u32 = 0x2000+0x4"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_MemoryExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_MemoryExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("value"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprMemoryEnforcesEqualsOperatorsByExpressionKind)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr poke = {0};
    Memmy_MemoryExpr scan = {0};

    Test_ParseMemoryExpr(arena, "0x1000 : u8 = 42", &poke);
    Test_ParseMemoryExpr(arena, "0x1000:+0x20 : u8 == 42", &scan);
    AssertEq(poke.kind, Memmy_MemoryExprKind_Poke);
    AssertEq(scan.kind, Memmy_MemoryExprKind_ValueScan);

    Memmy_MemoryExpr expr = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("0x1000:+0x20 : u8 = 42"), &expr, &error),
             Memmy_Status_ParseError);

    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("0x1000 : u8 == 42"), &expr, &error), Memmy_Status_ParseError);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprMemoryRejectsWhitespaceInsideRangeExpressions)
{
    String8 rejected[] = {
        String8_Lit("0x1000 :+ 0x20 : u8 == 1"),
        String8_Lit("<client.dll>[0x1000 :+ 0x4000]{90}"),
        String8_Lit("<client.dll>[0x1000 .. 0x5000]{90}"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_MemoryExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_MemoryExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("range"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprMemoryReportsByteOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_MemoryExpr expr = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("0x100 : u32 > 1"), &expr, &error), Memmy_Status_ParseError);
    AssertEq(error.byte_offset, 12);

    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("<client.dll>{48 gg}"), &expr, &error), Memmy_Status_ParseError);
    AssertEq(error.byte_offset, 16);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_dsl_memory =
    TestSuite_Make("Memmy DSL Memory", TestCase_Make(Test_MemmyExprMemoryDispatchesRequiredExamples),
                   TestCase_Make(Test_MemmyExprMemoryDispatchesEachTopLevelKind),
                   TestCase_Make(Test_MemmyExprMemoryParsesPatternScansWithWildcards),
                   TestCase_Make(Test_MemmyExprMemoryParsesPatternScanWithModuleConstSize),
                   TestCase_Make(Test_MemmyExprMemoryParsesExactValueScans),
                   TestCase_Make(Test_MemmyExprMemoryRejectsOrderingComparisons),
                   TestCase_Make(Test_MemmyExprMemoryRejectsBareWholeProcessTargets),
                   TestCase_Make(Test_MemmyExprMemoryParsesProcessQualifiedAddressSizedPatternScan),
                   TestCase_Make(Test_MemmyExprMemoryRejectsRhsAddressExpressionsForPokes),
                   TestCase_Make(Test_MemmyExprMemoryEnforcesEqualsOperatorsByExpressionKind),
                   TestCase_Make(Test_MemmyExprMemoryRejectsWhitespaceInsideRangeExpressions),
                   TestCase_Make(Test_MemmyExprMemoryReportsByteOffsets), );
