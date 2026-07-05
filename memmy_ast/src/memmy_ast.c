#include "memmy_ast.h"

Memmy_AstStatus Memmy_Ast_ParseExpr(Arena *arena, String8 text, Memmy_AstNode **out, Memmy_AstDiagnostic *diagnostic)
{
    Unused(arena);
    Unused(text);

    if (out != 0)
    {
        *out = 0;
    }

    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }

    return Memmy_AstStatus_Unsupported;
}

Memmy_AstStatus Memmy_Ast_ParseStatement(Arena *arena, String8 text, Memmy_AstStatement *out,
                                         Memmy_AstDiagnostic *diagnostic)
{
    Unused(arena);
    Unused(text);

    if (out != 0)
    {
        *out = (Memmy_AstStatement){0};
    }

    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }

    return Memmy_AstStatus_Unsupported;
}
