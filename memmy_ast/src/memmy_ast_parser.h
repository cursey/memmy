#ifndef MEMMY_AST_PARSER_H
#define MEMMY_AST_PARSER_H

#include "memmy_ast.h"

MemmyAst_Status MemmyAst_DisasmX64Pattern_Parse(Arena *arena, String8 input, String8 body, U64 body_offset,
                                                MemmyAst_DisasmPattern *out, MemmyAst_Diagnostic *diagnostic);

typedef U32 MemmyAst_TokenKind;
enum
{
    MemmyAst_TokenKind_Eof,
    MemmyAst_TokenKind_Invalid,
    MemmyAst_TokenKind_Identifier,
    MemmyAst_TokenKind_Variable,
    MemmyAst_TokenKind_CurrentItem,
    MemmyAst_TokenKind_Integer,
    MemmyAst_TokenKind_String,
    MemmyAst_TokenKind_Target,
    MemmyAst_TokenKind_Command,
    MemmyAst_TokenKind_As,
    MemmyAst_TokenKind_LBrace,
    MemmyAst_TokenKind_RBrace,
    MemmyAst_TokenKind_LBracket,
    MemmyAst_TokenKind_RBracket,
    MemmyAst_TokenKind_LParen,
    MemmyAst_TokenKind_RParen,
    MemmyAst_TokenKind_At,
    MemmyAst_TokenKind_Arrow,
    MemmyAst_TokenKind_FatArrow,
    MemmyAst_TokenKind_DotDot,
    MemmyAst_TokenKind_Plus,
    MemmyAst_TokenKind_Minus,
    MemmyAst_TokenKind_Star,
    MemmyAst_TokenKind_Slash,
    MemmyAst_TokenKind_Percent,
    MemmyAst_TokenKind_Equal,
    MemmyAst_TokenKind_EqualEqual,
    MemmyAst_TokenKind_ValuePipe,
};

typedef struct MemmyAst_Token MemmyAst_Token;
struct MemmyAst_Token
{
    MemmyAst_TokenKind kind;
    String8 text;
    U64 byte_offset;
    U64 byte_count;
};

typedef struct MemmyAst_Parser MemmyAst_Parser;
struct MemmyAst_Parser
{
    Arena *arena;
    String8 input;
    U64 pos;
    MemmyAst_Token token;
    MemmyAst_Diagnostic *diagnostic;
    U32 current_item_scope_depth;
};

void MemmyAst_Diagnostic_Set(MemmyAst_Diagnostic *diagnostic, String8 input, String8 context, String8 message,
                             U64 byte_offset, U64 byte_count);
void MemmyAst_Parser_SetError(MemmyAst_Parser *parser, String8 message, U64 byte_offset, U64 byte_count);
B32 MemmyAst_Parser_AtEnd(MemmyAst_Parser *parser);
U8 MemmyAst_Parser_Peek(MemmyAst_Parser *parser);
MemmyAst_Status MemmyAst_Parser_Next(MemmyAst_Parser *parser);
MemmyAst_Node *MemmyAst_Parser_PushNode(MemmyAst_Parser *parser, MemmyAst_NodeKind kind, MemmyAst_Token token);
B32 MemmyAst_Node_MayBeAddressLike(MemmyAst_Node *node);
void MemmyAst_Parser_NodeCoverInput(MemmyAst_Parser *parser, MemmyAst_Node *node, U64 start, U64 end);
B32 MemmyAst_Token_IsIdentifier(MemmyAst_Token token, String8 text);

MemmyAst_Status MemmyAst_Parser_ParseConstSum(MemmyAst_Parser *parser, MemmyAst_Node **out);
MemmyAst_Status MemmyAst_Parser_ParseExprAdditive(MemmyAst_Parser *parser, MemmyAst_Node **out);
MemmyAst_Status MemmyAst_Parser_ParsePostfix(MemmyAst_Parser *parser, MemmyAst_Node **out, B32 tight_only);
MemmyAst_Status MemmyAst_Parser_ParseExprOnly(MemmyAst_Parser *parser, MemmyAst_Node **out);

#endif
