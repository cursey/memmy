#include "memmy_ast.h"
#include "test_framework.h"

static void Test_ParseAstExpr(Arena *arena, char *text, Memmy_AstNode **out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
    AssertTrue(*out != 0);
}

static void Test_ParseAstStatement(Arena *arena, char *text, Memmy_AstStatement *out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseStatement(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
}

Test(Test_MemmyAstParsesConstantsWithPrecedence)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseAstExpr(arena, "0x1234 - 32 * (4 + 5)", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(expr->op, Memmy_AstConstOp_Sub);
    AssertEq(expr->value, 0x1234 - 32 * (4 + 5));
    AssertEq(expr->lhs->value, 0x1234);
    AssertEq(expr->rhs->op, Memmy_AstConstOp_Mul);
    AssertEq(expr->rhs->rhs->op, Memmy_AstConstOp_Add);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesVariablesInConstExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseAstExpr(arena, "$foo + 7 * $bar", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertTrue(expr->contains_variable);
    AssertEq(expr->op, Memmy_AstConstOp_Add);
    AssertEq(expr->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(expr->lhs->name, String8_Lit("foo"));
    AssertEq(expr->rhs->rhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(expr->rhs->rhs->name, String8_Lit("bar"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsInvalidIdentifiers)
{
    String8 rejected[] = {
        String8_Lit("$"),
        String8_Lit("$1"),
        String8_Lit("$foo-bar"),
        String8_Lit("client.dll"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AstNode *expr = 0;
        Memmy_AstDiagnostic diagnostic = {0};
        AssertEq(Memmy_Ast_ParseExpr(arena, rejected[i], &expr, &diagnostic), Memmy_AstStatus_ParseError);
        AssertTrue(expr == 0);
        AssertStrEq(diagnostic.context, String8_Lit("ast"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyAstParsesPocketReferenceTargets)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *process_name = 0;
    Test_ParseAstExpr(arena, "<game.exe!>", &process_name);
    AssertEq(process_name->kind, Memmy_AstNodeKind_Target);
    AssertTrue(process_name->target_has_process);
    AssertTrue(!process_name->target_process_is_pid);
    AssertStrEq(process_name->target_process, String8_Lit("game.exe"));
    AssertStrEq(process_name->target_module, String8_Lit(""));

    Memmy_AstNode *process_pid = 0;
    Test_ParseAstExpr(arena, "<1234!>", &process_pid);
    AssertTrue(process_pid->target_has_process);
    AssertTrue(process_pid->target_process_is_pid);
    AssertStrEq(process_pid->target_process, String8_Lit("1234"));

    Memmy_AstNode *module = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module);
    AssertTrue(!module->target_has_process);
    AssertStrEq(module->target_module, String8_Lit("client.dll"));

    Memmy_AstNode *process_module_name = 0;
    Test_ParseAstExpr(arena, "<game.exe!client.dll>", &process_module_name);
    AssertTrue(process_module_name->target_has_process);
    AssertStrEq(process_module_name->target_process, String8_Lit("game.exe"));
    AssertStrEq(process_module_name->target_module, String8_Lit("client.dll"));

    Memmy_AstNode *process_module_pid = 0;
    Test_ParseAstExpr(arena, "<1234!client.dll>", &process_module_pid);
    AssertTrue(process_module_pid->target_has_process);
    AssertTrue(process_module_pid->target_process_is_pid);
    AssertStrEq(process_module_pid->target_process, String8_Lit("1234"));
    AssertStrEq(process_module_pid->target_module, String8_Lit("client.dll"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesAssignmentBasics)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstStatement const_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = 42", &const_assignment);
    AssertEq(const_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertStrEq(const_assignment.assignment_name, String8_Lit("foo"));
    AssertEq(const_assignment.assignment_value->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(const_assignment.assignment_value->value, 42);

    Memmy_AstStatement variable_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = $bar", &variable_assignment);
    AssertEq(variable_assignment.assignment_value->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable_assignment.assignment_value->name, String8_Lit("bar"));

    Memmy_AstStatement target_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>", &target_assignment);
    AssertEq(target_assignment.assignment_value->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(target_assignment.assignment_value->target_module, String8_Lit("client.dll"));

    Arena_Destroy(arena);
}

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

Test(Test_MemmyAstKeepsLaterPhaseSyntaxOutOfScope)
{
    String8 rejected[] = {
        String8_Lit("@0x1234"),
        String8_Lit("[@0x1234..@0x5678]"),
        String8_Lit("<client.dll>+0x1234"),
        String8_Lit("<client.dll>{ab cd}"),
        String8_Lit("$foo[0]"),
        String8_Lit("/procs"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AstNode *expr = 0;
        Memmy_AstDiagnostic diagnostic = {0};
        AssertEq(Memmy_Ast_ParseExpr(arena, rejected[i], &expr, &diagnostic), Memmy_AstStatus_ParseError);
        Arena_Destroy(arena);
    }
}

TestSuite suite_memmy_ast = TestSuite_Make(
    "Memmy AST", TestCase_Make(Test_MemmyAstParsesConstantsWithPrecedence),
    TestCase_Make(Test_MemmyAstParsesVariablesInConstExpressions),
    TestCase_Make(Test_MemmyAstRejectsInvalidIdentifiers), TestCase_Make(Test_MemmyAstParsesPocketReferenceTargets),
    TestCase_Make(Test_MemmyAstParsesAssignmentBasics), TestCase_Make(Test_MemmyAstReportsPreciseDiagnosticOffsets),
    TestCase_Make(Test_MemmyAstKeepsLaterPhaseSyntaxOutOfScope), );
