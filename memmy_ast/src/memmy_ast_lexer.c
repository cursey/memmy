#include "memmy_ast_parser.h"

static B32 MemmyAst_Char_IsIdentStart(U8 c)
{
    return Char8_IsAlpha(c) || c == '_';
}

static B32 MemmyAst_Char_IsIdentContinue(U8 c)
{
    return MemmyAst_Char_IsIdentStart(c) || Char8_IsDigit(c);
}

static B32 MemmyAst_Char_IsHexDigit(U8 c)
{
    return Char8_IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

void MemmyAst_Diagnostic_Set(MemmyAst_Diagnostic *diagnostic, String8 input, String8 context, String8 message,
                             U64 byte_offset, U64 byte_count)
{
    if (diagnostic != 0)
    {
        *diagnostic = (MemmyAst_Diagnostic){
            .input = input,
            .message = message,
            .context = context,
            .byte_offset = byte_offset,
            .byte_count = byte_count,
        };
    }
}

void MemmyAst_Parser_SetError(MemmyAst_Parser *parser, String8 message, U64 byte_offset, U64 byte_count)
{
    MemmyAst_Diagnostic_Set(parser->diagnostic, parser->input, String8_Lit("ast"), message, byte_offset, byte_count);
}

B32 MemmyAst_Parser_AtEnd(MemmyAst_Parser *parser)
{
    return parser->pos >= parser->input.len;
}

U8 MemmyAst_Parser_Peek(MemmyAst_Parser *parser)
{
    U8 result = 0;
    if (!MemmyAst_Parser_AtEnd(parser))
    {
        result = parser->input.data[parser->pos];
    }
    return result;
}

static String8 MemmyAst_Parser_Slice(MemmyAst_Parser *parser, U64 start, U64 end)
{
    return String8_Substr(parser->input, start, end - start);
}

static void MemmyAst_Parser_SkipWhitespace(MemmyAst_Parser *parser)
{
    while (!MemmyAst_Parser_AtEnd(parser) && Char8_IsWhitespace(MemmyAst_Parser_Peek(parser)))
    {
        parser->pos++;
    }
}

MemmyAst_Status MemmyAst_Parser_Next(MemmyAst_Parser *parser)
{
    MemmyAst_Parser_SkipWhitespace(parser);

    U64 start = parser->pos;
    if (MemmyAst_Parser_AtEnd(parser))
    {
        parser->token = (MemmyAst_Token){.kind = MemmyAst_TokenKind_Eof, .byte_offset = start};
        return MemmyAst_Status_Ok;
    }

    U8 c = MemmyAst_Parser_Peek(parser);
    parser->pos++;
    MemmyAst_TokenKind kind = MemmyAst_TokenKind_Invalid;

    if (MemmyAst_Char_IsIdentStart(c))
    {
        while (!MemmyAst_Parser_AtEnd(parser) && MemmyAst_Char_IsIdentContinue(MemmyAst_Parser_Peek(parser)))
        {
            parser->pos++;
        }
        String8 text = MemmyAst_Parser_Slice(parser, start, parser->pos);
        kind = String8_Eq(text, String8_Lit("as")) ? MemmyAst_TokenKind_As : MemmyAst_TokenKind_Identifier;
    }
    else if (Char8_IsDigit(c))
    {
        B32 is_float = 0;
        if (c == '0' && (MemmyAst_Parser_Peek(parser) == 'x' || MemmyAst_Parser_Peek(parser) == 'X'))
        {
            parser->pos++;
            if (!MemmyAst_Char_IsHexDigit(MemmyAst_Parser_Peek(parser)))
            {
                MemmyAst_Parser_SetError(parser, String8_Lit("expected hexadecimal digit"), parser->pos, 1);
                return MemmyAst_Status_ParseError;
            }
            while (!MemmyAst_Parser_AtEnd(parser) && MemmyAst_Char_IsHexDigit(MemmyAst_Parser_Peek(parser)))
            {
                parser->pos++;
            }
        }
        else
        {
            while (!MemmyAst_Parser_AtEnd(parser) && Char8_IsDigit(MemmyAst_Parser_Peek(parser)))
            {
                parser->pos++;
            }
            if (!MemmyAst_Parser_AtEnd(parser) && MemmyAst_Parser_Peek(parser) == '.' &&
                (parser->pos + 1 >= parser->input.len || parser->input.data[parser->pos + 1] != '.'))
            {
                is_float = 1;
                parser->pos++;
                while (!MemmyAst_Parser_AtEnd(parser) && Char8_IsDigit(MemmyAst_Parser_Peek(parser)))
                {
                    parser->pos++;
                }
            }
            if (!MemmyAst_Parser_AtEnd(parser) &&
                (MemmyAst_Parser_Peek(parser) == 'e' || MemmyAst_Parser_Peek(parser) == 'E'))
            {
                is_float = 1;
                parser->pos++;
                if (!MemmyAst_Parser_AtEnd(parser) &&
                    (MemmyAst_Parser_Peek(parser) == '+' || MemmyAst_Parser_Peek(parser) == '-'))
                {
                    parser->pos++;
                }
                if (MemmyAst_Parser_AtEnd(parser) || !Char8_IsDigit(MemmyAst_Parser_Peek(parser)))
                {
                    MemmyAst_Parser_SetError(parser, String8_Lit("expected exponent digit"), parser->pos, 1);
                    return MemmyAst_Status_ParseError;
                }
                while (!MemmyAst_Parser_AtEnd(parser) && Char8_IsDigit(MemmyAst_Parser_Peek(parser)))
                {
                    parser->pos++;
                }
            }
        }
        kind = is_float ? MemmyAst_TokenKind_Float : MemmyAst_TokenKind_Integer;
    }
    else if (c == '$')
    {
        if (MemmyAst_Parser_AtEnd(parser))
        {
            kind = MemmyAst_TokenKind_CurrentItem;
        }
        else if (MemmyAst_Char_IsIdentStart(MemmyAst_Parser_Peek(parser)))
        {
            while (!MemmyAst_Parser_AtEnd(parser) && MemmyAst_Char_IsIdentContinue(MemmyAst_Parser_Peek(parser)))
            {
                parser->pos++;
            }
            kind = MemmyAst_TokenKind_Variable;
        }
        else if (Char8_IsDigit(MemmyAst_Parser_Peek(parser)))
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("invalid variable name"), start, parser->pos - start);
            return MemmyAst_Status_ParseError;
        }
        else
        {
            kind = MemmyAst_TokenKind_CurrentItem;
        }
    }
    else if (c == '"')
    {
        B32 terminated = 0;
        while (!MemmyAst_Parser_AtEnd(parser))
        {
            U8 next = MemmyAst_Parser_Peek(parser);
            parser->pos++;
            if (next == '\\' && !MemmyAst_Parser_AtEnd(parser))
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
            MemmyAst_Parser_SetError(parser, String8_Lit("unterminated string"), start, parser->pos - start);
            return MemmyAst_Status_ParseError;
        }
        kind = MemmyAst_TokenKind_String;
    }
    else if (c == '<')
    {
        B32 terminated = 0;
        while (!MemmyAst_Parser_AtEnd(parser))
        {
            if (MemmyAst_Parser_Peek(parser) == '>')
            {
                parser->pos++;
                terminated = 1;
                break;
            }
            parser->pos++;
        }
        if (!terminated)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("unterminated target"), start, parser->pos - start);
            return MemmyAst_Status_ParseError;
        }
        kind = MemmyAst_TokenKind_Target;
    }
    else if (c == '/')
    {
        if (MemmyAst_Char_IsIdentStart(MemmyAst_Parser_Peek(parser)))
        {
            while (!MemmyAst_Parser_AtEnd(parser) && MemmyAst_Char_IsIdentContinue(MemmyAst_Parser_Peek(parser)))
            {
                parser->pos++;
            }
            kind = MemmyAst_TokenKind_Command;
        }
        else
        {
            kind = MemmyAst_TokenKind_Slash;
        }
    }
    else if (c == '-' && MemmyAst_Parser_Peek(parser) == '>')
    {
        parser->pos++;
        kind = MemmyAst_TokenKind_Arrow;
    }
    else if (c == '.' && MemmyAst_Parser_Peek(parser) == '.')
    {
        parser->pos++;
        kind = MemmyAst_TokenKind_DotDot;
    }
    else if (c == '=' && MemmyAst_Parser_Peek(parser) == '=')
    {
        parser->pos++;
        kind = MemmyAst_TokenKind_EqualEqual;
    }
    else if (c == '=' && MemmyAst_Parser_Peek(parser) == '>')
    {
        parser->pos++;
        kind = MemmyAst_TokenKind_FatArrow;
    }
    else if (c == '|' && MemmyAst_Parser_Peek(parser) == '>')
    {
        parser->pos++;
        kind = MemmyAst_TokenKind_ValuePipe;
    }
    else if (c == '{')
    {
        kind = MemmyAst_TokenKind_LBrace;
    }
    else if (c == '}')
    {
        kind = MemmyAst_TokenKind_RBrace;
    }
    else if (c == '[')
    {
        kind = MemmyAst_TokenKind_LBracket;
    }
    else if (c == ']')
    {
        kind = MemmyAst_TokenKind_RBracket;
    }
    else if (c == '(')
    {
        kind = MemmyAst_TokenKind_LParen;
    }
    else if (c == ')')
    {
        kind = MemmyAst_TokenKind_RParen;
    }
    else if (c == '@')
    {
        kind = MemmyAst_TokenKind_At;
    }
    else if (c == '+')
    {
        kind = MemmyAst_TokenKind_Plus;
    }
    else if (c == '-')
    {
        kind = MemmyAst_TokenKind_Minus;
    }
    else if (c == '*')
    {
        kind = MemmyAst_TokenKind_Star;
    }
    else if (c == '%')
    {
        kind = MemmyAst_TokenKind_Percent;
    }
    else if (c == '=')
    {
        kind = MemmyAst_TokenKind_Equal;
    }

    if (kind == MemmyAst_TokenKind_Invalid)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unexpected character"), start, 1);
        return MemmyAst_Status_ParseError;
    }

    parser->token = (MemmyAst_Token){
        .kind = kind,
        .text = MemmyAst_Parser_Slice(parser, start, parser->pos),
        .byte_offset = start,
        .byte_count = parser->pos - start,
    };
    return MemmyAst_Status_Ok;
}

MemmyAst_Node *MemmyAst_Parser_PushNode(MemmyAst_Parser *parser, MemmyAst_NodeKind kind, MemmyAst_Token token)
{
    MemmyAst_Node *node = Arena_PushStruct(parser->arena, MemmyAst_Node);
    node->kind = kind;
    node->text = token.text;
    node->byte_offset = token.byte_offset;
    node->byte_count = token.byte_count;
    return node;
}

B32 MemmyAst_Node_MayBeAddressLike(MemmyAst_Node *node)
{
    B32 result = 0;
    if (node != 0)
    {
        if (node->kind == MemmyAst_NodeKind_Address || node->kind == MemmyAst_NodeKind_Range ||
            node->kind == MemmyAst_NodeKind_ProcessRange || node->kind == MemmyAst_NodeKind_Target ||
            node->kind == MemmyAst_NodeKind_Deref || node->kind == MemmyAst_NodeKind_Function ||
            node->kind == MemmyAst_NodeKind_ObjectBase || node->kind == MemmyAst_NodeKind_Variable ||
            node->kind == MemmyAst_NodeKind_CurrentItem || node->kind == MemmyAst_NodeKind_ValuePipe)
        {
            result = 1;
        }
        else if (node->kind == MemmyAst_NodeKind_ConstArithmetic)
        {
            result = node->contains_variable || MemmyAst_Node_MayBeAddressLike(node->lhs) ||
                     MemmyAst_Node_MayBeAddressLike(node->rhs);
        }
    }
    return result;
}

void MemmyAst_Parser_NodeCoverInput(MemmyAst_Parser *parser, MemmyAst_Node *node, U64 start, U64 end)
{
    node->byte_offset = start;
    node->byte_count = end - start;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
}

B32 MemmyAst_Token_IsIdentifier(MemmyAst_Token token, String8 text)
{
    return token.kind == MemmyAst_TokenKind_Identifier && String8_Eq(token.text, text);
}
