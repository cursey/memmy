#include "memmy_ast_parser.h"

static B32 Memmy_Char_IsIdentStart(U8 c)
{
    return Char8_IsAlpha(c) || c == '_';
}

static B32 Memmy_Char_IsIdentContinue(U8 c)
{
    return Memmy_Char_IsIdentStart(c) || Char8_IsDigit(c);
}

static B32 Memmy_Char_IsHexDigit(U8 c)
{
    return Char8_IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

void Memmy_AstDiagnostic_Set(Memmy_AstDiagnostic *diagnostic, String8 input, String8 context, String8 message,
                             U64 byte_offset, U64 byte_count)
{
    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){
            .input = input,
            .message = message,
            .context = context,
            .byte_offset = byte_offset,
            .byte_count = byte_count,
        };
    }
}

void Memmy_Parser_SetError(Memmy_Parser *parser, String8 message, U64 byte_offset, U64 byte_count)
{
    Memmy_AstDiagnostic_Set(parser->diagnostic, parser->input, String8_Lit("ast"), message, byte_offset, byte_count);
}

B32 Memmy_Parser_AtEnd(Memmy_Parser *parser)
{
    return parser->pos >= parser->input.len;
}

U8 Memmy_Parser_Peek(Memmy_Parser *parser)
{
    U8 result = 0;
    if (!Memmy_Parser_AtEnd(parser))
    {
        result = parser->input.data[parser->pos];
    }
    return result;
}

static String8 Memmy_Parser_Slice(Memmy_Parser *parser, U64 start, U64 end)
{
    return String8_Substr(parser->input, start, end - start);
}

static void Memmy_Parser_SkipWhitespace(Memmy_Parser *parser)
{
    while (!Memmy_Parser_AtEnd(parser) && Char8_IsWhitespace(Memmy_Parser_Peek(parser)))
    {
        parser->pos++;
    }
}

Memmy_AstStatus Memmy_Parser_Next(Memmy_Parser *parser)
{
    Memmy_Parser_SkipWhitespace(parser);

    U64 start = parser->pos;
    if (Memmy_Parser_AtEnd(parser))
    {
        parser->token = (Memmy_Token){.kind = Memmy_TokenKind_Eof, .byte_offset = start};
        return Memmy_AstStatus_Ok;
    }

    U8 c = Memmy_Parser_Peek(parser);
    parser->pos++;
    Memmy_TokenKind kind = Memmy_TokenKind_Invalid;

    if (Memmy_Char_IsIdentStart(c))
    {
        while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsIdentContinue(Memmy_Parser_Peek(parser)))
        {
            parser->pos++;
        }
        String8 text = Memmy_Parser_Slice(parser, start, parser->pos);
        kind = String8_Eq(text, String8_Lit("as")) ? Memmy_TokenKind_As : Memmy_TokenKind_Identifier;
    }
    else if (Char8_IsDigit(c))
    {
        if (c == '0' && (Memmy_Parser_Peek(parser) == 'x' || Memmy_Parser_Peek(parser) == 'X'))
        {
            parser->pos++;
            if (!Memmy_Char_IsHexDigit(Memmy_Parser_Peek(parser)))
            {
                Memmy_Parser_SetError(parser, String8_Lit("expected hexadecimal digit"), parser->pos, 1);
                return Memmy_AstStatus_ParseError;
            }
            while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsHexDigit(Memmy_Parser_Peek(parser)))
            {
                parser->pos++;
            }
        }
        else
        {
            while (!Memmy_Parser_AtEnd(parser) && Char8_IsDigit(Memmy_Parser_Peek(parser)))
            {
                parser->pos++;
            }
        }
        kind = Memmy_TokenKind_Integer;
    }
    else if (c == '$')
    {
        if (Memmy_Parser_AtEnd(parser))
        {
            kind = Memmy_TokenKind_CurrentItem;
        }
        else if (Memmy_Char_IsIdentStart(Memmy_Parser_Peek(parser)))
        {
            while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsIdentContinue(Memmy_Parser_Peek(parser)))
            {
                parser->pos++;
            }
            kind = Memmy_TokenKind_Variable;
        }
        else if (Char8_IsDigit(Memmy_Parser_Peek(parser)))
        {
            Memmy_Parser_SetError(parser, String8_Lit("invalid variable name"), start, parser->pos - start);
            return Memmy_AstStatus_ParseError;
        }
        else
        {
            kind = Memmy_TokenKind_CurrentItem;
        }
    }
    else if (c == '"')
    {
        B32 terminated = 0;
        while (!Memmy_Parser_AtEnd(parser))
        {
            U8 next = Memmy_Parser_Peek(parser);
            parser->pos++;
            if (next == '\\' && !Memmy_Parser_AtEnd(parser))
            {
                parser->pos++;
            }
            else if (next == '"')
            {
                terminated = 1;
                break;
            }
        }
        if (!terminated)
        {
            Memmy_Parser_SetError(parser, String8_Lit("unterminated string"), start, parser->pos - start);
            return Memmy_AstStatus_ParseError;
        }
        kind = Memmy_TokenKind_String;
    }
    else if (c == '<')
    {
        B32 terminated = 0;
        while (!Memmy_Parser_AtEnd(parser))
        {
            if (Memmy_Parser_Peek(parser) == '>')
            {
                parser->pos++;
                terminated = 1;
                break;
            }
            parser->pos++;
        }
        if (!terminated)
        {
            Memmy_Parser_SetError(parser, String8_Lit("unterminated target"), start, parser->pos - start);
            return Memmy_AstStatus_ParseError;
        }
        kind = Memmy_TokenKind_Target;
    }
    else if (c == '/')
    {
        if (Memmy_Char_IsIdentStart(Memmy_Parser_Peek(parser)))
        {
            while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsIdentContinue(Memmy_Parser_Peek(parser)))
            {
                parser->pos++;
            }
            kind = Memmy_TokenKind_Command;
        }
        else
        {
            kind = Memmy_TokenKind_Slash;
        }
    }
    else if (c == '-' && Memmy_Parser_Peek(parser) == '>')
    {
        parser->pos++;
        kind = Memmy_TokenKind_Arrow;
    }
    else if (c == '.' && Memmy_Parser_Peek(parser) == '.')
    {
        parser->pos++;
        kind = Memmy_TokenKind_DotDot;
    }
    else if (c == '=' && Memmy_Parser_Peek(parser) == '=')
    {
        parser->pos++;
        kind = Memmy_TokenKind_EqualEqual;
    }
    else if (c == '=' && Memmy_Parser_Peek(parser) == '>')
    {
        parser->pos++;
        kind = Memmy_TokenKind_FatArrow;
    }
    else if (c == '{')
    {
        kind = Memmy_TokenKind_LBrace;
    }
    else if (c == '}')
    {
        kind = Memmy_TokenKind_RBrace;
    }
    else if (c == '[')
    {
        kind = Memmy_TokenKind_LBracket;
    }
    else if (c == ']')
    {
        kind = Memmy_TokenKind_RBracket;
    }
    else if (c == '(')
    {
        kind = Memmy_TokenKind_LParen;
    }
    else if (c == ')')
    {
        kind = Memmy_TokenKind_RParen;
    }
    else if (c == '@')
    {
        kind = Memmy_TokenKind_At;
    }
    else if (c == '+')
    {
        kind = Memmy_TokenKind_Plus;
    }
    else if (c == '-')
    {
        kind = Memmy_TokenKind_Minus;
    }
    else if (c == '*')
    {
        kind = Memmy_TokenKind_Star;
    }
    else if (c == '%')
    {
        kind = Memmy_TokenKind_Percent;
    }
    else if (c == '=')
    {
        kind = Memmy_TokenKind_Equal;
    }

    if (kind == Memmy_TokenKind_Invalid)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unexpected character"), start, 1);
        return Memmy_AstStatus_ParseError;
    }

    parser->token = (Memmy_Token){
        .kind = kind,
        .text = Memmy_Parser_Slice(parser, start, parser->pos),
        .byte_offset = start,
        .byte_count = parser->pos - start,
    };
    return Memmy_AstStatus_Ok;
}

Memmy_AstNode *Memmy_Parser_PushNode(Memmy_Parser *parser, Memmy_AstNodeKind kind, Memmy_Token token)
{
    Memmy_AstNode *node = Arena_PushStruct(parser->arena, Memmy_AstNode);
    node->kind = kind;
    node->text = token.text;
    node->byte_offset = token.byte_offset;
    node->byte_count = token.byte_count;
    return node;
}

B32 Memmy_AstNode_MayBeAddressLike(Memmy_AstNode *node)
{
    B32 result = 0;
    if (node != 0)
    {
        if (node->kind == Memmy_AstNodeKind_Address || node->kind == Memmy_AstNodeKind_Range ||
            node->kind == Memmy_AstNodeKind_ProcessRange || node->kind == Memmy_AstNodeKind_Target ||
            node->kind == Memmy_AstNodeKind_Deref || node->kind == Memmy_AstNodeKind_Function ||
            node->kind == Memmy_AstNodeKind_ObjectBase || node->kind == Memmy_AstNodeKind_Variable ||
            node->kind == Memmy_AstNodeKind_CurrentItem)
        {
            result = 1;
        }
        else if (node->kind == Memmy_AstNodeKind_ConstArithmetic)
        {
            result = node->contains_variable || Memmy_AstNode_MayBeAddressLike(node->lhs) ||
                     Memmy_AstNode_MayBeAddressLike(node->rhs);
        }
    }
    return result;
}

void Memmy_Parser_NodeCoverInput(Memmy_Parser *parser, Memmy_AstNode *node, U64 start, U64 end)
{
    node->byte_offset = start;
    node->byte_count = end - start;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
}

B32 Memmy_Token_IsIdentifier(Memmy_Token token, String8 text)
{
    return token.kind == Memmy_TokenKind_Identifier && String8_Eq(token.text, text);
}
