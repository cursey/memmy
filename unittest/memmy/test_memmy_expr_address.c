#include "memmy_expr.h"
#include "test_framework.h"

static void Test_ParseAddressExpr(Arena *arena, char *text, Memmy_AddressExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_AddressExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static Memmy_AddressOp *Test_AddressOpAt(Memmy_AddressExpr *expr, U64 index)
{
    U64 at = 0;
    List_ForEach(Memmy_AddressOp, op, &expr->ops, link)
    {
        if (at == index)
        {
            return op;
        }
        at++;
    }
    return 0;
}

Test(Test_MemmyExprAddressParsesAbsoluteAddressBases)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddressExpr(arena, "0x000001d856780004", &expr);

    AssertEq(expr.base_kind, Memmy_AddressExprBaseKind_Absolute);
    AssertEq(expr.absolute, 0x000001d856780004ull);
    AssertEq(expr.ops.count, 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprAddressParsesModuleTargetBases)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddressExpr(arena, "<client.dll>", &expr);

    AssertEq(expr.base_kind, Memmy_AddressExprBaseKind_Target);
    AssertEq(expr.target.kind, Memmy_TargetExprKind_Module);
    AssertEq(expr.target.process.kind, Memmy_ProcessSelectorKind_None);
    AssertStrEq(expr.target.module_name, String8_Lit("client.dll"));
    AssertEq(expr.ops.count, 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprAddressParsesAddAndSubOperations)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddressExpr(arena, "<client.dll>+0x123-32", &expr);

    AssertEq(expr.ops.count, 2);

    Memmy_AddressOp *add = Test_AddressOpAt(&expr, 0);
    AssertTrue(add != 0);
    AssertEq(add->kind, Memmy_AddressOpKind_Add);
    AssertEq(add->offset, 0x123);

    Memmy_AddressOp *sub = Test_AddressOpAt(&expr, 1);
    AssertTrue(sub != 0);
    AssertEq(sub->kind, Memmy_AddressOpKind_Sub);
    AssertEq(sub->offset, 32);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprAddressParsesDerefAndDerefOffsetOperations)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddressExpr(arena, "<client.dll>+0x123->->0x8", &expr);

    AssertEq(expr.ops.count, 3);

    Memmy_AddressOp *add = Test_AddressOpAt(&expr, 0);
    AssertTrue(add != 0);
    AssertEq(add->kind, Memmy_AddressOpKind_Add);
    AssertEq(add->offset, 0x123);

    Memmy_AddressOp *deref = Test_AddressOpAt(&expr, 1);
    AssertTrue(deref != 0);
    AssertEq(deref->kind, Memmy_AddressOpKind_Deref);

    Memmy_AddressOp *deref_offset = Test_AddressOpAt(&expr, 2);
    AssertTrue(deref_offset != 0);
    AssertEq(deref_offset->kind, Memmy_AddressOpKind_DerefOffset);
    AssertEq(deref_offset->offset, 0x8);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprAddressParsesParenthesizedNegativeOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddressExpr(arena, "<client.dll>+0x4242->(8 * 0x30)->(-0x4)", &expr);

    AssertEq(expr.ops.count, 3);

    Memmy_AddressOp *add = Test_AddressOpAt(&expr, 0);
    AssertTrue(add != 0);
    AssertEq(add->kind, Memmy_AddressOpKind_Add);
    AssertEq(add->offset, 0x4242);

    Memmy_AddressOp *scaled = Test_AddressOpAt(&expr, 1);
    AssertTrue(scaled != 0);
    AssertEq(scaled->kind, Memmy_AddressOpKind_DerefOffset);
    AssertEq(scaled->offset, 8 * 0x30);

    Memmy_AddressOp *negative = Test_AddressOpAt(&expr, 2);
    AssertTrue(negative != 0);
    AssertEq(negative->kind, Memmy_AddressOpKind_DerefOffset);
    AssertEq(negative->offset, -0x4);

    Arena_Destroy(arena);
}

Test(Test_MemmyExprAddressRejectsInvalidWhitespace)
{
    struct
    {
        String8 text;
        U64 byte_offset;
    } rejected[] = {
        {String8_Lit("0x100 +0x8"), 5},
        {String8_Lit("0x100+ 0x8"), 6},
        {String8_Lit("<client.dll> ->0x8"), 12},
        {String8_Lit("<client.dll>+0x8 "), 16},
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AddressExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_AddressExpr_Parse(arena, rejected[i].text, &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        AssertEq(error.byte_offset, rejected[i].byte_offset);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprAddressRejectsMalformedPointerChainSyntax)
{
    String8 rejected[] = {
        String8_Lit("<client.dll>->("),
        String8_Lit("<client.dll>->(8 * )"),
        String8_Lit("<client.dll>- >0x8"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AddressExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_AddressExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        AssertStrEq(error.input, rejected[i]);
        AssertTrue(error.byte_offset <= rejected[i].len);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprAddressRejectsAddressRanges)
{
    String8 rejected[] = {
        String8_Lit("0x100..0x200"),
        String8_Lit("<client.dll>+0x100..0x200"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AddressExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_AddressExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        AssertTrue(error.byte_offset > 0);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyExprAddressRejectsWholeProcessTargets)
{
    String8 rejected[] = {
        String8_Lit("<game.exe!>"),
        String8_Lit("<123!>"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AddressExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_AddressExpr_Parse(arena, rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
        AssertStrEq(error.input, rejected[i]);
        AssertEq(error.byte_offset, 0);
        AssertEq(error.byte_count, rejected[i].len);
        Arena_Destroy(arena);
    }
}

TestSuite suite_memmy_expr_address =
    TestSuite_Make("Memmy Expr Address", TestCase_Make(Test_MemmyExprAddressParsesAbsoluteAddressBases),
                   TestCase_Make(Test_MemmyExprAddressParsesModuleTargetBases),
                   TestCase_Make(Test_MemmyExprAddressParsesAddAndSubOperations),
                   TestCase_Make(Test_MemmyExprAddressParsesDerefAndDerefOffsetOperations),
                   TestCase_Make(Test_MemmyExprAddressParsesParenthesizedNegativeOffsets),
                   TestCase_Make(Test_MemmyExprAddressRejectsInvalidWhitespace),
                   TestCase_Make(Test_MemmyExprAddressRejectsMalformedPointerChainSyntax),
                   TestCase_Make(Test_MemmyExprAddressRejectsAddressRanges),
                   TestCase_Make(Test_MemmyExprAddressRejectsWholeProcessTargets), );
