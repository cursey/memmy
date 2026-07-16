#include "base.h"

#include "test_memmy_ast_common.h"

Test(Test_MemmyAstReportsPreciseDiagnosticOffsets)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};

    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("$foo + "), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertEq(diagnostic.byte_offset, 7);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("0xg"), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertEq(diagnostic.byte_offset, 2);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("1 2"), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertEq(diagnostic.byte_offset, 2);
    AssertEq(diagnostic.byte_count, 1);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsOldAddressSpellings)
{
    Test_RejectAstExpr("0x1234->");
    Test_RejectAstExpr("address:+size");
    Test_RejectAstExpr("<1234!>0x1234");
    Test_RejectAstExpr("[<1234!a.dll>..<b.dll>]");
    Test_RejectAstExpr("[<a.dll>..<1234!b.dll>]");
}

Test(Test_MemmyAstCopiesInputForNodesAndDiagnostics)
{
    Arena *arena = Arena_CreateDefault();
    AssertEq(MemmyAst_DisasmArch_Null, 0);
    U8 expression_text[] = "$stable";
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_Make(expression_text, sizeof(expression_text) - 1), &expr, &diagnostic),
             MemmyAst_Status_Ok);
    Memory_Set(expression_text, 'x', sizeof(expression_text) - 1);
    AssertStrEq(expr->name, String8_Lit("stable"));
    AssertStrEq(expr->text, String8_Lit("$stable"));

    U8 invalid_text[] = "$broken + ";
    AssertEq(MemmyAst_Expr_Parse(arena, String8_Make(invalid_text, sizeof(invalid_text) - 1), &expr, &diagnostic),
             MemmyAst_Status_ParseError);
    Memory_Set(invalid_text, 'x', sizeof(invalid_text) - 1);
    AssertStrEq(diagnostic.input, String8_Lit("$broken + "));
    AssertStrEq(diagnostic.message, String8_Lit("expected expression"));

    expr = (MemmyAst_Node *)1;
    diagnostic = (MemmyAst_Diagnostic){.byte_offset = 99};
    AssertEq(MemmyAst_Expr_Parse(0, String8_Lit("1"), &expr, &diagnostic), MemmyAst_Status_InvalidArgument);
    AssertTrue(expr == 0);
    AssertEq(diagnostic.byte_offset, 0);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_diagnostics =
    TestSuite_Make("Memmy AST Diagnostics", TestCase_Make(Test_MemmyAstReportsPreciseDiagnosticOffsets),
                   TestCase_Make(Test_MemmyAstRejectsOldAddressSpellings),
                   TestCase_Make(Test_MemmyAstCopiesInputForNodesAndDiagnostics), );
