#ifndef MEMMY_AST_PARSER_H
#define MEMMY_AST_PARSER_H

#include "memmy_ast.h"

typedef U32 Memmy_TokenKind;
enum
{
    Memmy_TokenKind_Eof,
    Memmy_TokenKind_Invalid,
    Memmy_TokenKind_Identifier,
    Memmy_TokenKind_Variable,
    Memmy_TokenKind_CurrentItem,
    Memmy_TokenKind_Integer,
    Memmy_TokenKind_String,
    Memmy_TokenKind_Target,
    Memmy_TokenKind_Command,
    Memmy_TokenKind_As,
    Memmy_TokenKind_LBrace,
    Memmy_TokenKind_RBrace,
    Memmy_TokenKind_LBracket,
    Memmy_TokenKind_RBracket,
    Memmy_TokenKind_LParen,
    Memmy_TokenKind_RParen,
    Memmy_TokenKind_At,
    Memmy_TokenKind_Arrow,
    Memmy_TokenKind_FatArrow,
    Memmy_TokenKind_DotDot,
    Memmy_TokenKind_Plus,
    Memmy_TokenKind_Minus,
    Memmy_TokenKind_Star,
    Memmy_TokenKind_Slash,
    Memmy_TokenKind_Percent,
    Memmy_TokenKind_Equal,
    Memmy_TokenKind_EqualEqual,
};

typedef struct Memmy_Token Memmy_Token;
struct Memmy_Token
{
    Memmy_TokenKind kind;
    String8 text;
    U64 byte_offset;
    U64 byte_count;
};

typedef struct Memmy_Parser Memmy_Parser;
struct Memmy_Parser
{
    Arena *arena;
    String8 input;
    U64 pos;
    Memmy_Token token;
    Memmy_AstDiagnostic *diagnostic;
    U32 current_item_scope_depth;
};

void Memmy_AstDiagnostic_Set(Memmy_AstDiagnostic *diagnostic, String8 input, String8 context, String8 message,
                             U64 byte_offset, U64 byte_count);
void Memmy_Parser_SetError(Memmy_Parser *parser, String8 message, U64 byte_offset, U64 byte_count);
B32 Memmy_Parser_AtEnd(Memmy_Parser *parser);
U8 Memmy_Parser_Peek(Memmy_Parser *parser);
Memmy_AstStatus Memmy_Parser_Next(Memmy_Parser *parser);
Memmy_AstNode *Memmy_Parser_PushNode(Memmy_Parser *parser, Memmy_AstNodeKind kind, Memmy_Token token);
B32 Memmy_AstNode_MayBeAddressLike(Memmy_AstNode *node);
void Memmy_Parser_NodeCoverInput(Memmy_Parser *parser, Memmy_AstNode *node, U64 start, U64 end);
B32 Memmy_Token_IsIdentifier(Memmy_Token token, String8 text);

Memmy_AstStatus Memmy_Parser_ParseConstSum(Memmy_Parser *parser, Memmy_AstNode **out);
Memmy_AstStatus Memmy_Parser_ParseExprAdditive(Memmy_Parser *parser, Memmy_AstNode **out);
Memmy_AstStatus Memmy_Parser_ParsePostfix(Memmy_Parser *parser, Memmy_AstNode **out, B32 tight_only);
Memmy_AstStatus Memmy_Parser_ParseExprOnly(Memmy_Parser *parser, Memmy_AstNode **out);

#endif
