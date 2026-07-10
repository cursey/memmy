#ifndef TEST_MEMMY_AST_COMMON_H
#define TEST_MEMMY_AST_COMMON_H

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

static void Test_RejectAstExpr(char *text)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertTrue(expr == 0);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    Arena_Destroy(arena);
}

#endif
