#include "memmy_expr.h"
#include "test_framework.h"

static void Test_AssertConstExpr(char *text, I64 expected)
{
    Memmy_ConstExpr expr = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_ConstExpr_Evaluate(String8_FromCStr(text), &expr, &error), Memmy_Status_Ok);
    AssertTrue(expr.value == expected);
}

Test(Test_MemmyExprConstEvaluatesPrecedenceAndParentheses)
{
    Test_AssertConstExpr("1 + 2 * 3", 7);
    Test_AssertConstExpr("(1 + 2) * 3", 9);
    Test_AssertConstExpr("0x10 + 10 / 2 - 3", 18);
    Test_AssertConstExpr("18 / 5", 3);
    Test_AssertConstExpr("18 % 5", 3);
    Test_AssertConstExpr("-1 + +2", 1);
    Test_AssertConstExpr("-(2 + 3) * 4", -20);
    Test_AssertConstExpr(" -0x8000000000000000 ", I64_MIN);
}

Test(Test_MemmyExprConstRejectsDivisionAndModuloByZero)
{
    String8 rejected[] = {
        String8_Lit("1 / 0"),
        String8_Lit("1 % 0"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_ConstExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_ConstExpr_Evaluate(rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
    }
}

Test(Test_MemmyExprConstRejectsInvalidSyntax)
{
    String8 rejected[] = {
        String8_Lit(""),   String8_Lit("1 +"), String8_Lit("(1 + 2"),     String8_Lit("1 2"),
        String8_Lit("0x"), String8_Lit("0xg"), String8_Lit("client.dll"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_ConstExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_ConstExpr_Evaluate(rejected[i], &expr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("expr"));
    }
}

Test(Test_MemmyExprConstRejectsOverflow)
{
    String8 rejected[] = {
        String8_Lit("9223372036854775808"),     String8_Lit("0x8000000000000000"),
        String8_Lit("9223372036854775807 + 1"), String8_Lit("-9223372036854775808 - 1"),
        String8_Lit("3037000500 * 3037000500"), String8_Lit("-9223372036854775808 / -1"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_ConstExpr expr = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_ConstExpr_Evaluate(rejected[i], &expr, &error), Memmy_Status_Overflow);
        AssertStrEq(error.context, String8_Lit("expr"));
    }
}

TestSuite suite_memmy_expr_const = TestSuite_Make(
    "Memmy Expr Const", TestCase_Make(Test_MemmyExprConstEvaluatesPrecedenceAndParentheses),
    TestCase_Make(Test_MemmyExprConstRejectsDivisionAndModuloByZero),
    TestCase_Make(Test_MemmyExprConstRejectsInvalidSyntax), TestCase_Make(Test_MemmyExprConstRejectsOverflow), );
