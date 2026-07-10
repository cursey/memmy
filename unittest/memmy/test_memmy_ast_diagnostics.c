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

TestSuite suite_memmy_ast_diagnostics =
    TestSuite_Make("Memmy AST Diagnostics", TestCase_Make(Test_MemmyAstReportsPreciseDiagnosticOffsets),
                   TestCase_Make(Test_MemmyAstRejectsOldAddressSpellings), );
