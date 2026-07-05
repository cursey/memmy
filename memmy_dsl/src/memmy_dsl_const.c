#include "memmy_dsl.h"

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

typedef struct Memmy_ConstAstParser Memmy_ConstAstParser;
struct Memmy_ConstAstParser
{
    Arena *arena;
    String8 text;
    U64 pos;
    Memmy_Error *error;
};

static B32 Memmy_ConstAstParser_AtEnd(Memmy_ConstAstParser *parser)
{
    return parser->pos >= parser->text.len;
}

static U8 Memmy_ConstAstParser_Peek(Memmy_ConstAstParser *parser)
{
    U8 result = 0;
    if (!Memmy_ConstAstParser_AtEnd(parser))
    {
        result = parser->text.data[parser->pos];
    }
    return result;
}

static void Memmy_ConstAstParser_SkipWhitespace(Memmy_ConstAstParser *parser)
{
    while (!Memmy_ConstAstParser_AtEnd(parser))
    {
        U8 c = Memmy_ConstAstParser_Peek(parser);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
            break;
        }
        parser->pos++;
    }
}

static B32 Memmy_VariableRef_IsIdentStart(U8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static B32 Memmy_VariableRef_IsIdentContinue(U8 c)
{
    return Memmy_VariableRef_IsIdentStart(c) || Char8_IsDigit(c);
}

static Memmy_Status Memmy_ConstAstParser_ParseVariable(Memmy_ConstAstParser *parser, Memmy_VariableRef *out)
{
    U64 start = parser->pos;
    if (Memmy_ConstAstParser_Peek(parser) != '$')
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected variable"), parser->text, parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    parser->pos++;
    if (Memmy_ConstAstParser_AtEnd(parser) || !Memmy_VariableRef_IsIdentStart(Memmy_ConstAstParser_Peek(parser)))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("invalid variable name"), parser->text, start, parser->pos - start);
        return Memmy_Status_ParseError;
    }

    U64 name_start = parser->pos;
    while (!Memmy_ConstAstParser_AtEnd(parser) && Memmy_VariableRef_IsIdentContinue(Memmy_ConstAstParser_Peek(parser)))
    {
        parser->pos++;
    }

    *out = (Memmy_VariableRef){
        .name = String8_Substr(parser->text, name_start, parser->pos - name_start),
    };
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstAstParser_ParseSum(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out);

static Memmy_Status Memmy_ConstAstParser_ParseUnsignedLiteral(Memmy_ConstAstParser *parser, U64 limit,
                                                              Memmy_ConstExpr *out)
{
    U64 start = parser->pos;
    U32 base = 10;
    if (parser->pos + 1 < parser->text.len && parser->text.data[parser->pos] == '0' &&
        (parser->text.data[parser->pos + 1] == 'x' || parser->text.data[parser->pos + 1] == 'X'))
    {
        base = 16;
        parser->pos += 2;
        if (Memmy_ConstAstParser_AtEnd(parser) ||
            Memmy_ConstParser_HexDigitValue(Memmy_ConstAstParser_Peek(parser)) >= 16)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("expected hexadecimal digit"), parser->text, parser->pos, 1);
            return Memmy_Status_ParseError;
        }
    }
    else if (Memmy_ConstAstParser_AtEnd(parser) || !Char8_IsDigit(Memmy_ConstAstParser_Peek(parser)))
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

    *out = (Memmy_ConstExpr){
        .kind = Memmy_ConstExprKind_Literal,
        .value = (I64)value,
    };
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstAstParser_ParsePrimary(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out)
{
    Memmy_ConstAstParser_SkipWhitespace(parser);
    U8 c = Memmy_ConstAstParser_Peek(parser);
    if (c == '(')
    {
        parser->pos++;
        Memmy_Status status = Memmy_ConstAstParser_ParseSum(parser, out);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_ConstAstParser_SkipWhitespace(parser);
        if (Memmy_ConstAstParser_Peek(parser) != ')')
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("expected ')'"), parser->text, parser->pos, 1);
            return Memmy_Status_ParseError;
        }
        parser->pos++;
        return Memmy_Status_Ok;
    }

    if (c == '$')
    {
        Memmy_VariableRef variable = {0};
        Memmy_Status status = Memmy_ConstAstParser_ParseVariable(parser, &variable);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_ConstExpr){
            .kind = Memmy_ConstExprKind_Variable,
            .contains_variable = 1,
            .variable = variable,
        };
        return Memmy_Status_Ok;
    }

    return Memmy_ConstAstParser_ParseUnsignedLiteral(parser, (U64)I64_MAX, out);
}

static Memmy_Status Memmy_ConstAstParser_ParseUnary(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out)
{
    Memmy_ConstAstParser_SkipWhitespace(parser);
    U8 c = Memmy_ConstAstParser_Peek(parser);
    if (c == '+' || c == '-')
    {
        parser->pos++;
        if (c == '-')
        {
            Memmy_ConstAstParser_SkipWhitespace(parser);
            if (Char8_IsDigit(Memmy_ConstAstParser_Peek(parser)))
            {
                Memmy_ConstExpr literal = {0};
                Memmy_Status status = Memmy_ConstAstParser_ParseUnsignedLiteral(parser, (U64)I64_MAX + 1ull, &literal);
                if (status != Memmy_Status_Ok)
                {
                    return status;
                }
                if ((U64)literal.value == (U64)I64_MAX + 1ull)
                {
                    literal.value = I64_MIN;
                }
                else
                {
                    literal.value = -literal.value;
                }
                *out = literal;
                return Memmy_Status_Ok;
            }
        }

        Memmy_ConstExpr *operand = Arena_PushStruct(parser->arena, Memmy_ConstExpr);
        Memmy_Status status = Memmy_ConstAstParser_ParseUnary(parser, operand);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        I64 value = operand->value;
        if (c == '-' && !operand->contains_variable && !SubI64Checked(0, operand->value, &value))
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("constant expression overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }

        *out = (Memmy_ConstExpr){
            .kind = Memmy_ConstExprKind_Unary,
            .value = value,
            .contains_variable = operand->contains_variable,
            .op = c,
            .lhs = operand,
        };
        return Memmy_Status_Ok;
    }

    return Memmy_ConstAstParser_ParsePrimary(parser, out);
}

static Memmy_Status Memmy_ConstAstParser_Combine(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out, U8 op,
                                                 Memmy_ConstExpr *rhs)
{
    Memmy_ConstExpr *lhs = Arena_PushStruct(parser->arena, Memmy_ConstExpr);
    *lhs = *out;

    B32 contains_variable = lhs->contains_variable || rhs->contains_variable;
    I64 value = 0;
    if (!contains_variable)
    {
        B32 ok = 1;
        if (op == '+')
        {
            ok = AddI64Checked(lhs->value, rhs->value, &value);
        }
        else if (op == '-')
        {
            ok = SubI64Checked(lhs->value, rhs->value, &value);
        }
        else if (op == '*')
        {
            ok = MulI64Checked(lhs->value, rhs->value, &value);
        }
        else if (op == '/')
        {
            if (rhs->value == 0)
            {
                Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                         String8_Lit("division by zero"), parser->text, parser->pos, 1);
                return Memmy_Status_ParseError;
            }
            ok = DivI64Checked(lhs->value, rhs->value, &value);
        }
        else if (op == '%')
        {
            if (rhs->value == 0)
            {
                Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                         String8_Lit("modulo by zero"), parser->text, parser->pos, 1);
                return Memmy_Status_ParseError;
            }
            ok = ModI64Checked(lhs->value, rhs->value, &value);
        }
        if (!ok)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("constant expression overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
    }

    *out = (Memmy_ConstExpr){
        .kind = Memmy_ConstExprKind_Binary,
        .value = value,
        .contains_variable = contains_variable,
        .op = op,
        .lhs = lhs,
        .rhs = rhs,
    };
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstAstParser_ParseProduct(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out)
{
    Memmy_Status status = Memmy_ConstAstParser_ParseUnary(parser, out);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    for (;;)
    {
        Memmy_ConstAstParser_SkipWhitespace(parser);
        U8 op = Memmy_ConstAstParser_Peek(parser);
        if (op != '*' && op != '/' && op != '%')
        {
            break;
        }
        parser->pos++;

        Memmy_ConstExpr *rhs = Arena_PushStruct(parser->arena, Memmy_ConstExpr);
        status = Memmy_ConstAstParser_ParseUnary(parser, rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_ConstAstParser_Combine(parser, out, op, rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ConstAstParser_ParseSum(Memmy_ConstAstParser *parser, Memmy_ConstExpr *out)
{
    Memmy_Status status = Memmy_ConstAstParser_ParseProduct(parser, out);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    for (;;)
    {
        Memmy_ConstAstParser_SkipWhitespace(parser);
        U8 op = Memmy_ConstAstParser_Peek(parser);
        if (op != '+' && op != '-')
        {
            break;
        }
        parser->pos++;

        Memmy_ConstExpr *rhs = Arena_PushStruct(parser->arena, Memmy_ConstExpr);
        status = Memmy_ConstAstParser_ParseProduct(parser, rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_ConstAstParser_Combine(parser, out, op, rhs);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    return Memmy_Status_Ok;
}

Memmy_Status Memmy_ConstExpr_Parse(Arena *arena, String8 text, Memmy_ConstExpr *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing arena or output const expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ConstAstParser parser = {
        .arena = arena,
        .text = text,
        .error = error,
    };
    Memmy_ConstExpr result = {0};
    Memmy_Status status = Memmy_ConstAstParser_ParseSum(&parser, &result);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ConstAstParser_SkipWhitespace(&parser);
    if (!Memmy_ConstAstParser_AtEnd(&parser))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("unexpected trailing input"), text, parser.pos, 1);
        return Memmy_Status_ParseError;
    }

    *out = result;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
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

    *out = (Memmy_ConstExpr){.kind = Memmy_ConstExprKind_Literal, .value = value};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
