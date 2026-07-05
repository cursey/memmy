#include "memmy_ast.h"

#include "base_checked.h"

typedef U32 Memmy_TokenKind;
enum
{
    Memmy_TokenKind_Eof,
    Memmy_TokenKind_Invalid,
    Memmy_TokenKind_Identifier,
    Memmy_TokenKind_Variable,
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
};

static B32 Memmy_Char_IsIdentStart(U8 c)
{
    return Char8_IsAlpha(c) || c == '_';
}

static B32 Memmy_Char_IsIdentContinue(U8 c)
{
    return Memmy_Char_IsIdentStart(c) || Char8_IsDigit(c);
}

static B32 Memmy_Char_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static B32 Memmy_Char_IsHexDigit(U8 c)
{
    return Char8_IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static U32 Memmy_Char_HexDigitValue(U8 c)
{
    U32 result = U32_MAX;
    if (c >= '0' && c <= '9')
    {
        result = (U32)(c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
        result = 10u + (U32)(c - 'a');
    }
    else if (c >= 'A' && c <= 'F')
    {
        result = 10u + (U32)(c - 'A');
    }
    return result;
}

static void Memmy_AstDiagnostic_Set(Memmy_AstDiagnostic *diagnostic, String8 input, String8 context, String8 message,
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

static void Memmy_Parser_SetError(Memmy_Parser *parser, String8 message, U64 byte_offset, U64 byte_count)
{
    Memmy_AstDiagnostic_Set(parser->diagnostic, parser->input, String8_Lit("ast"), message, byte_offset, byte_count);
}

static B32 Memmy_Parser_AtEnd(Memmy_Parser *parser)
{
    return parser->pos >= parser->input.len;
}

static U8 Memmy_Parser_Peek(Memmy_Parser *parser)
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
    while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsWhitespace(Memmy_Parser_Peek(parser)))
    {
        parser->pos++;
    }
}

static Memmy_AstStatus Memmy_Parser_Next(Memmy_Parser *parser)
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
        if (!Memmy_Char_IsIdentStart(Memmy_Parser_Peek(parser)))
        {
            Memmy_Parser_SetError(parser, String8_Lit("invalid variable name"), start, parser->pos - start);
            return Memmy_AstStatus_ParseError;
        }
        while (!Memmy_Parser_AtEnd(parser) && Memmy_Char_IsIdentContinue(Memmy_Parser_Peek(parser)))
        {
            parser->pos++;
        }
        kind = Memmy_TokenKind_Variable;
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

static Memmy_AstNode *Memmy_Parser_PushNode(Memmy_Parser *parser, Memmy_AstNodeKind kind, Memmy_Token token)
{
    Memmy_AstNode *node = Arena_PushStruct(parser->arena, Memmy_AstNode);
    node->kind = kind;
    node->text = token.text;
    node->byte_offset = token.byte_offset;
    node->byte_count = token.byte_count;
    return node;
}

static Memmy_AstStatus Memmy_Parser_ParseConstSum(Memmy_Parser *parser, Memmy_AstNode **out);

static Memmy_AstStatus Memmy_Parser_ParseIntegerValue(Memmy_Parser *parser, Memmy_Token token, U64 limit, I64 *out)
{
    U64 pos = 0;
    U32 base = 10;
    if (token.text.len >= 2 && token.text.data[0] == '0' && (token.text.data[1] == 'x' || token.text.data[1] == 'X'))
    {
        base = 16;
        pos = 2;
    }

    U64 value = 0;
    for (; pos < token.text.len; pos++)
    {
        U8 c = token.text.data[pos];
        U32 digit = (base == 16) ? Memmy_Char_HexDigitValue(c) : (U32)(c - '0');
        if (value > (limit - digit) / base)
        {
            Memmy_Parser_SetError(parser, String8_Lit("integer literal overflow"), token.byte_offset + pos, 1);
            return Memmy_AstStatus_Overflow;
        }
        value = value * base + digit;
    }

    *out = (I64)value;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseTargetNode(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_Token token = parser->token;
    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Target, token);
    String8 body = String8_Substr(token.text, 1, token.text.len - 2);
    if (body.len == 0)
    {
        Memmy_Parser_SetError(parser, String8_Lit("empty target"), token.byte_offset + 1, 1);
        return Memmy_AstStatus_ParseError;
    }

    U64 bang = STRING8_NPOS;
    for (U64 i = 0; i < body.len; i++)
    {
        U8 c = body.data[i];
        if (Memmy_Char_IsWhitespace(c))
        {
            Memmy_Parser_SetError(parser, String8_Lit("invalid target"), token.byte_offset + 1 + i, 1);
            return Memmy_AstStatus_ParseError;
        }
        if (c == '!')
        {
            if (bang != STRING8_NPOS)
            {
                Memmy_Parser_SetError(parser, String8_Lit("invalid target"), token.byte_offset + 1 + i, 1);
                return Memmy_AstStatus_ParseError;
            }
            bang = i;
        }
    }

    if (bang == STRING8_NPOS)
    {
        node->target_module = body;
    }
    else
    {
        if (bang == 0)
        {
            Memmy_Parser_SetError(parser, String8_Lit("missing target process"), token.byte_offset + 1, 1);
            return Memmy_AstStatus_ParseError;
        }
        node->target_has_process = 1;
        node->target_process = String8_Substr(body, 0, bang);
        node->target_module = String8_Substr(body, bang + 1, body.len - bang - 1);
        node->target_process_is_pid = 1;
        for (U64 i = 0; i < node->target_process.len; i++)
        {
            if (!Char8_IsDigit(node->target_process.data[i]))
            {
                node->target_process_is_pid = 0;
                break;
            }
        }
    }

    *out = node;
    return Memmy_Parser_Next(parser);
}

static Memmy_AstStatus Memmy_Parser_ParseConstPrimary(Memmy_Parser *parser, Memmy_AstNode **out)
{
    if (parser->token.kind == Memmy_TokenKind_Integer)
    {
        Memmy_Token token = parser->token;
        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ConstArithmetic, token);
        node->op = Memmy_AstConstOp_None;
        Memmy_AstStatus status = Memmy_Parser_ParseIntegerValue(parser, token, (U64)I64_MAX, &node->value);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        *out = node;
        return Memmy_Parser_Next(parser);
    }

    if (parser->token.kind == Memmy_TokenKind_Variable)
    {
        Memmy_Token token = parser->token;
        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Variable, token);
        node->name = String8_Substr(token.text, 1, token.text.len - 1);
        node->contains_variable = 1;
        *out = node;
        return Memmy_Parser_Next(parser);
    }

    if (parser->token.kind == Memmy_TokenKind_LParen)
    {
        U64 start = parser->token.byte_offset;
        Memmy_AstStatus status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        status = Memmy_Parser_ParseConstSum(parser, out);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        if (parser->token.kind != Memmy_TokenKind_RParen)
        {
            Memmy_Parser_SetError(parser, String8_Lit("expected ')'"), parser->token.byte_offset, 1);
            return Memmy_AstStatus_ParseError;
        }
        (*out)->byte_offset = start;
        (*out)->byte_count = parser->token.byte_offset + parser->token.byte_count - start;
        return Memmy_Parser_Next(parser);
    }

    Memmy_Parser_SetError(parser, String8_Lit("expected expression"), parser->token.byte_offset, 1);
    return Memmy_AstStatus_ParseError;
}

static Memmy_AstStatus Memmy_Parser_ParseConstUnary(Memmy_Parser *parser, Memmy_AstNode **out)
{
    if (parser->token.kind == Memmy_TokenKind_Plus || parser->token.kind == Memmy_TokenKind_Minus)
    {
        Memmy_Token token = parser->token;
        Memmy_AstStatus status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        if (token.kind == Memmy_TokenKind_Minus && parser->token.kind == Memmy_TokenKind_Integer)
        {
            Memmy_Token literal = parser->token;
            Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ConstArithmetic, token);
            node->op = Memmy_AstConstOp_None;
            node->text = String8_Substr(parser->input, token.byte_offset,
                                        literal.byte_offset + literal.byte_count - token.byte_offset);
            node->byte_count = node->text.len;
            status = Memmy_Parser_ParseIntegerValue(parser, literal, (U64)I64_MAX + 1ull, &node->value);
            if (status != Memmy_AstStatus_Ok)
            {
                return status;
            }
            if ((U64)node->value == (U64)I64_MAX + 1ull)
            {
                node->value = I64_MIN;
            }
            else
            {
                node->value = -node->value;
            }
            *out = node;
            return Memmy_Parser_Next(parser);
        }

        Memmy_AstNode *operand = 0;
        status = Memmy_Parser_ParseConstUnary(parser, &operand);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ConstArithmetic, token);
        node->op = (token.kind == Memmy_TokenKind_Plus) ? Memmy_AstConstOp_Pos : Memmy_AstConstOp_Neg;
        node->lhs = operand;
        node->contains_variable = operand->contains_variable;
        node->byte_count = operand->byte_offset + operand->byte_count - token.byte_offset;
        if (!node->contains_variable)
        {
            if (node->op == Memmy_AstConstOp_Neg && !SubI64Checked(0, operand->value, &node->value))
            {
                Memmy_Parser_SetError(parser, String8_Lit("constant expression overflow"), token.byte_offset, 1);
                return Memmy_AstStatus_Overflow;
            }
            else if (node->op == Memmy_AstConstOp_Pos)
            {
                node->value = operand->value;
            }
            else
            {
                node->value = -operand->value;
            }
        }
        *out = node;
        return Memmy_AstStatus_Ok;
    }

    return Memmy_Parser_ParseConstPrimary(parser, out);
}

static Memmy_AstStatus Memmy_Parser_CombineConst(Memmy_Parser *parser, Memmy_AstNode **out, Memmy_Token op_token,
                                                 Memmy_AstNode *rhs)
{
    Memmy_AstNode *lhs = *out;
    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ConstArithmetic, op_token);
    node->lhs = lhs;
    node->rhs = rhs;
    node->byte_offset = lhs->byte_offset;
    node->byte_count = rhs->byte_offset + rhs->byte_count - lhs->byte_offset;
    node->contains_variable = lhs->contains_variable || rhs->contains_variable;

    switch (op_token.kind)
    {
    case Memmy_TokenKind_Plus:
        node->op = Memmy_AstConstOp_Add;
        break;
    case Memmy_TokenKind_Minus:
        node->op = Memmy_AstConstOp_Sub;
        break;
    case Memmy_TokenKind_Star:
        node->op = Memmy_AstConstOp_Mul;
        break;
    case Memmy_TokenKind_Slash:
        node->op = Memmy_AstConstOp_Div;
        break;
    case Memmy_TokenKind_Percent:
        node->op = Memmy_AstConstOp_Mod;
        break;
    default:
        break;
    }

    if (!node->contains_variable)
    {
        B32 ok = 1;
        if (node->op == Memmy_AstConstOp_Add)
        {
            ok = AddI64Checked(lhs->value, rhs->value, &node->value);
        }
        else if (node->op == Memmy_AstConstOp_Sub)
        {
            ok = SubI64Checked(lhs->value, rhs->value, &node->value);
        }
        else if (node->op == Memmy_AstConstOp_Mul)
        {
            ok = MulI64Checked(lhs->value, rhs->value, &node->value);
        }
        else if (node->op == Memmy_AstConstOp_Div)
        {
            if (rhs->value == 0)
            {
                Memmy_Parser_SetError(parser, String8_Lit("division by zero"), op_token.byte_offset, 1);
                return Memmy_AstStatus_ParseError;
            }
            ok = DivI64Checked(lhs->value, rhs->value, &node->value);
        }
        else if (node->op == Memmy_AstConstOp_Mod)
        {
            if (rhs->value == 0)
            {
                Memmy_Parser_SetError(parser, String8_Lit("modulo by zero"), op_token.byte_offset, 1);
                return Memmy_AstStatus_ParseError;
            }
            ok = ModI64Checked(lhs->value, rhs->value, &node->value);
        }

        if (!ok)
        {
            Memmy_Parser_SetError(parser, String8_Lit("constant expression overflow"), op_token.byte_offset, 1);
            return Memmy_AstStatus_Overflow;
        }
    }

    *out = node;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseConstProduct(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseConstUnary(parser, out);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    while (parser->token.kind == Memmy_TokenKind_Star || parser->token.kind == Memmy_TokenKind_Slash ||
           parser->token.kind == Memmy_TokenKind_Percent)
    {
        Memmy_Token op = parser->token;
        status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        Memmy_AstNode *rhs = 0;
        status = Memmy_Parser_ParseConstUnary(parser, &rhs);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        status = Memmy_Parser_CombineConst(parser, out, op, rhs);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
    }

    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseConstSum(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseConstProduct(parser, out);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    while (parser->token.kind == Memmy_TokenKind_Plus || parser->token.kind == Memmy_TokenKind_Minus)
    {
        Memmy_Token op = parser->token;
        status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        Memmy_AstNode *rhs = 0;
        status = Memmy_Parser_ParseConstProduct(parser, &rhs);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        status = Memmy_Parser_CombineConst(parser, out, op, rhs);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
    }

    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseExprOnly(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_AstStatus_Ok;
    if (parser->token.kind == Memmy_TokenKind_Target)
    {
        status = Memmy_Parser_ParseTargetNode(parser, out);
    }
    else
    {
        status = Memmy_Parser_ParseConstSum(parser, out);
    }
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind != Memmy_TokenKind_Eof)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    return Memmy_AstStatus_Ok;
}

Memmy_AstStatus Memmy_Ast_ParseExpr(Arena *arena, String8 text, Memmy_AstNode **out, Memmy_AstDiagnostic *diagnostic)
{
    if (out != 0)
    {
        *out = 0;
    }
    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_AstDiagnostic_Set(diagnostic, text, String8_Lit("ast"), String8_Lit("missing arena or output"), 0, 0);
        return Memmy_AstStatus_InvalidArgument;
    }

    Memmy_Parser parser = {
        .arena = arena,
        .input = text,
        .diagnostic = diagnostic,
    };
    Memmy_AstStatus status = Memmy_Parser_Next(&parser);
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParseExprOnly(&parser, out);
    }
    if (status != Memmy_AstStatus_Ok && out != 0)
    {
        *out = 0;
    }
    if (status == Memmy_AstStatus_Ok && diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }
    return status;
}

Memmy_AstStatus Memmy_Ast_ParseStatement(Arena *arena, String8 text, Memmy_AstStatement *out,
                                         Memmy_AstDiagnostic *diagnostic)
{
    if (out != 0)
    {
        *out = (Memmy_AstStatement){0};
    }
    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_AstDiagnostic_Set(diagnostic, text, String8_Lit("ast"), String8_Lit("missing arena or output"), 0, 0);
        return Memmy_AstStatus_InvalidArgument;
    }

    Memmy_Parser parser = {
        .arena = arena,
        .input = text,
        .diagnostic = diagnostic,
    };
    Memmy_AstStatus status = Memmy_Parser_Next(&parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    out->text = text;
    if (parser.token.kind == Memmy_TokenKind_Variable)
    {
        Memmy_Token variable = parser.token;
        status = Memmy_Parser_Next(&parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        if (parser.token.kind == Memmy_TokenKind_Equal)
        {
            out->kind = Memmy_AstNodeKind_Assignment;
            out->assignment_name = String8_Substr(variable.text, 1, variable.text.len - 1);
            status = Memmy_Parser_Next(&parser);
            if (status != Memmy_AstStatus_Ok)
            {
                *out = (Memmy_AstStatement){0};
                return status;
            }
            status = Memmy_Parser_ParseExprOnly(&parser, &out->assignment_value);
            if (status != Memmy_AstStatus_Ok)
            {
                *out = (Memmy_AstStatement){0};
                return status;
            }
            return Memmy_AstStatus_Ok;
        }

        parser.pos = variable.byte_offset + variable.byte_count;
        parser.token = variable;
    }

    status = Memmy_Parser_ParseExprOnly(&parser, &out->expr);
    if (status != Memmy_AstStatus_Ok)
    {
        *out = (Memmy_AstStatement){0};
        return status;
    }
    out->kind = out->expr->kind;
    return Memmy_AstStatus_Ok;
}
