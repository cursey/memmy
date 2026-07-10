#include "test_memmy_ast_common.h"

Test(Test_MemmyAstReportsPreciseDiagnosticOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("$foo + "), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(diagnostic.byte_offset, 7);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("0xg"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(diagnostic.byte_offset, 2);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("1 2"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
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
    AssertEq(Memmy_AstDisasmArch_Null, 0);
    U8 expression_text[] = "$stable";
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Make(expression_text, sizeof(expression_text) - 1), &expr, &diagnostic),
             Memmy_AstStatus_Ok);
    Memory_Set(expression_text, 'x', sizeof(expression_text) - 1);
    AssertStrEq(expr->name, String8_Lit("stable"));
    AssertStrEq(expr->text, String8_Lit("$stable"));

    U8 invalid_text[] = "$broken + ";
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Make(invalid_text, sizeof(invalid_text) - 1), &expr, &diagnostic),
             Memmy_AstStatus_ParseError);
    Memory_Set(invalid_text, 'x', sizeof(invalid_text) - 1);
    AssertStrEq(diagnostic.input, String8_Lit("$broken + "));
    AssertStrEq(diagnostic.message, String8_Lit("expected expression"));

    expr = (Memmy_AstNode *)1;
    diagnostic = (Memmy_AstDiagnostic){.byte_offset = 99};
    AssertEq(Memmy_Ast_ParseExpr(0, String8_Lit("1"), &expr, &diagnostic), Memmy_AstStatus_InvalidArgument);
    AssertTrue(expr == 0);
    AssertEq(diagnostic.byte_offset, 0);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_diagnostics =
    TestSuite_Make("Memmy AST Diagnostics", TestCase_Make(Test_MemmyAstReportsPreciseDiagnosticOffsets),
                   TestCase_Make(Test_MemmyAstRejectsOldAddressSpellings),
                   TestCase_Make(Test_MemmyAstCopiesInputForNodesAndDiagnostics), );
