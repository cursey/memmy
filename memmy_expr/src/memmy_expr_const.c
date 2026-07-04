#include "memmy_expr.h"

#include "base_checked.h"

typedef struct Memmy_ConstParser Memmy_ConstParser;
struct Memmy_ConstParser
{
    String8 text;
    U64 pos;
    Memmy_Error *error;
};

static void Memmy_ExprError_SetInput(Memmy_Error *error, Memmy_Status status, String8 context, String8 message,
                                     String8 input, U64 byte_offset, U64 byte_count)
{
    if (error != 0)
    {
        *error = (Memmy_Error){
            .status = status,
            .message = message,
            .input = input,
            .byte_offset = byte_offset,
            .byte_count = byte_count,
            .context = context,
        };
    }
}

static B32 Memmy_ConstParser_AtEnd(Memmy_ConstParser *parser)
{
    return parser->pos >= parser->text.len;
}

static U8 Memmy_ConstParser_Peek(Memmy_ConstParser *parser)
{
    U8 result = 0;
    if (!Memmy_ConstParser_AtEnd(parser))
    {
        result = parser->text.data[parser->pos];
    }
    return result;
}

static void Memmy_ConstParser_SkipWhitespace(Memmy_ConstParser *parser)
{
    while (!Memmy_ConstParser_AtEnd(parser))
    {
        U8 c = Memmy_ConstParser_Peek(parser);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
            break;
        }
        parser->pos++;
    }
}

static U32 Memmy_ConstParser_HexDigitValue(U8 c)
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

static Memmy_Status Memmy_ConstParser_ParseSum(Memmy_ConstParser *parser, I64 *out);

static Memmy_Status Memmy_ConstParser_ParseUnsignedLiteral(Memmy_ConstParser *parser, U64 limit, U64 *out)
{
    U64 start = parser->pos;
    U32 base = 10;
    if (parser->pos + 1 < parser->text.len && parser->text.data[parser->pos] == '0' &&
        (parser->text.data[parser->pos + 1] == 'x' || parser->text.data[parser->pos + 1] == 'X'))
    {
        base = 16;
        parser->pos += 2;
        if (Memmy_ConstParser_AtEnd(parser) || Memmy_ConstParser_HexDigitValue(Memmy_ConstParser_Peek(parser)) >= 16)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("expected hexadecimal digit"), parser->text, parser->pos, 1);
            return Memmy_Status_ParseError;
        }
    }
    else if (Memmy_ConstParser_AtEnd(parser) || !Char8_IsDigit(Memmy_ConstParser_Peek(parser)))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected integer"), parser->text, parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    U64 value = 0;
    for (; parser->pos < parser->text.len; parser->pos++)
    {
        U8 c = parser->text.data[parser->pos];
        U32 digit = (base == 16) ? Memmy_ConstParser_HexDigitValue(c) : (Char8_IsDigit(c) ? (U32)(c - '0') : U32_MAX);
        if (digit >= base)
        {
            break;
        }

        if (value > (limit - digit) / base)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("integer literal overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
        value = value * base + digit;
    }

    if (parser->pos == start)
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected integer"), parser->text, parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    *out = value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstParser_ParsePrimary(Memmy_ConstParser *parser, I64 *out)
{
    Memmy_ConstParser_SkipWhitespace(parser);
    if (Memmy_ConstParser_Peek(parser) == '(')
    {
        parser->pos++;
        Memmy_Status status = Memmy_ConstParser_ParseSum(parser, out);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_ConstParser_SkipWhitespace(parser);
        if (Memmy_ConstParser_Peek(parser) != ')')
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("expected ')'"), parser->text, parser->pos, 1);
            return Memmy_Status_ParseError;
        }
        parser->pos++;
        return Memmy_Status_Ok;
    }

    U64 value = 0;
    Memmy_Status status = Memmy_ConstParser_ParseUnsignedLiteral(parser, (U64)I64_MAX, &value);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = (I64)value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstParser_ParseUnary(Memmy_ConstParser *parser, I64 *out)
{
    Memmy_ConstParser_SkipWhitespace(parser);
    U8 c = Memmy_ConstParser_Peek(parser);
    if (c == '+')
    {
        parser->pos++;
        return Memmy_ConstParser_ParseUnary(parser, out);
    }
    if (c == '-')
    {
        parser->pos++;
        Memmy_ConstParser_SkipWhitespace(parser);
        U8 next = Memmy_ConstParser_Peek(parser);
        if (Char8_IsDigit(next))
        {
            U64 value = 0;
            Memmy_Status status = Memmy_ConstParser_ParseUnsignedLiteral(parser, (U64)I64_MAX + 1ull, &value);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (value == (U64)I64_MAX + 1ull)
            {
                *out = I64_MIN;
            }
            else
            {
                *out = -(I64)value;
            }
            return Memmy_Status_Ok;
        }

        I64 value = 0;
        Memmy_Status status = Memmy_ConstParser_ParseUnary(parser, &value);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (!SubI64Checked(0, value, out))
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("constant expression overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
        return Memmy_Status_Ok;
    }

    return Memmy_ConstParser_ParsePrimary(parser, out);
}

static Memmy_Status Memmy_ConstParser_ParseProduct(Memmy_ConstParser *parser, I64 *out)
{
    Memmy_Status status = Memmy_ConstParser_ParseUnary(parser, out);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    for (;;)
    {
        Memmy_ConstParser_SkipWhitespace(parser);
        U8 op = Memmy_ConstParser_Peek(parser);
        if (op != '*' && op != '/' && op != '%')
        {
            break;
        }
        parser->pos++;

        I64 rhs = 0;
        status = Memmy_ConstParser_ParseUnary(parser, &rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        I64 result = 0;
        if (op == '*')
        {
            if (!MulI64Checked(*out, rhs, &result))
            {
                status = Memmy_Status_Overflow;
            }
        }
        else if (op == '/')
        {
            if (rhs == 0)
            {
                Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                         String8_Lit("division by zero"), parser->text, parser->pos, 1);
                return Memmy_Status_ParseError;
            }
            if (!DivI64Checked(*out, rhs, &result))
            {
                status = Memmy_Status_Overflow;
            }
        }
        else
        {
            if (rhs == 0)
            {
                Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                         String8_Lit("modulo by zero"), parser->text, parser->pos, 1);
                return Memmy_Status_ParseError;
            }
            if (!ModI64Checked(*out, rhs, &result))
            {
                status = Memmy_Status_Overflow;
            }
        }

        if (status == Memmy_Status_Overflow)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("constant expression overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
        *out = result;
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstParser_ParseSum(Memmy_ConstParser *parser, I64 *out)
{
    Memmy_Status status = Memmy_ConstParser_ParseProduct(parser, out);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    for (;;)
    {
        Memmy_ConstParser_SkipWhitespace(parser);
        U8 op = Memmy_ConstParser_Peek(parser);
        if (op != '+' && op != '-')
        {
            break;
        }
        parser->pos++;

        I64 rhs = 0;
        status = Memmy_ConstParser_ParseProduct(parser, &rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        I64 result = 0;
        B32 ok = (op == '+') ? AddI64Checked(*out, rhs, &result) : SubI64Checked(*out, rhs, &result);
        if (!ok)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("constant expression overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
        *out = result;
    }

    return Memmy_Status_Ok;
}

Memmy_Status Memmy_ConstExpr_Evaluate(String8 text, Memmy_ConstExpr *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"), String8_Lit("missing output expr"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ConstParser parser = {
        .text = text,
        .error = error,
    };
    I64 value = 0;
    Memmy_Status status = Memmy_ConstParser_ParseSum(&parser, &value);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ConstParser_SkipWhitespace(&parser);
    if (!Memmy_ConstParser_AtEnd(&parser))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("unexpected trailing input"), text, parser.pos, 1);
        return Memmy_Status_ParseError;
    }

    *out = (Memmy_ConstExpr){.value = value};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
