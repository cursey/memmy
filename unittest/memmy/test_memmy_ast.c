#include "memmy_ast.h"
#include "test_framework.h"

Test(Test_MemmyAstSmokeParsesAreStubbed)
{
    Memmy_AstNode *expr = (Memmy_AstNode *)1;
    Memmy_AstStatement statement = {0};
    Memmy_AstDiagnostic diagnostic = {0};

    AssertEq(Memmy_Ast_ParseExpr(0, String8_Lit(""), &expr, &diagnostic), Memmy_AstStatus_Unsupported);
    AssertTrue(expr == 0);
    AssertEq(Memmy_Ast_ParseStatement(0, String8_Lit(""), &statement, &diagnostic), Memmy_AstStatus_Unsupported);
    AssertEq(statement.kind, Memmy_AstNodeKind_Null);
}

TestSuite suite_memmy_ast = TestSuite_Make("Memmy AST", TestCase_Make(Test_MemmyAstSmokeParsesAreStubbed), );
