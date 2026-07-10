#ifndef TEST_MEMMY_AST_COMMON_H
#define TEST_MEMMY_AST_COMMON_H

#include "memmy_ast.h"
#include "test_framework.h"

static void Test_ParseAstExpr(Arena *arena, char *text, MemmyAst_Node **out)
{
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_FromCStr(text), out, &diagnostic), MemmyAst_Status_Ok);
    AssertTrue(*out != 0);
}

static void Test_ParseAstStatement(Arena *arena, char *text, MemmyAst_Statement *out)
{
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Statement_Parse(arena, String8_FromCStr(text), out, &diagnostic), MemmyAst_Status_Ok);
}

static void Test_RejectAstExpr(char *text)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_FromCStr(text), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertTrue(expr == 0);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    Arena_Destroy(arena);
}

#endif
