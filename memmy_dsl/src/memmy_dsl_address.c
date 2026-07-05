#include "memmy_dsl.h"

typedef struct Memmy_AddressParser Memmy_AddressParser;
struct Memmy_AddressParser
{
    Arena *arena;
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

static B32 Memmy_AddressParser_AtEnd(Memmy_AddressParser *parser)
{
    return parser->pos >= parser->text.len;
}

static U8 Memmy_AddressParser_Peek(Memmy_AddressParser *parser)
{
    U8 result = 0;
    if (!Memmy_AddressParser_AtEnd(parser))
    {
        result = parser->text.data[parser->pos];
    }
    return result;
}

static B32 Memmy_AddressParser_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static B32 Memmy_VariableRef_IsIdentStart(U8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static B32 Memmy_VariableRef_IsIdentContinue(U8 c)
{
    return Memmy_VariableRef_IsIdentStart(c) || Char8_IsDigit(c);
}

static Memmy_Status Memmy_AddressParser_ParseVariable(Memmy_AddressParser *parser, Memmy_VariableRef *out)
{
    U64 start = parser->pos;
    if (Memmy_AddressParser_Peek(parser) != '$')
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected variable"), parser->text, parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    parser->pos++;
    if (Memmy_AddressParser_AtEnd(parser) || !Memmy_VariableRef_IsIdentStart(Memmy_AddressParser_Peek(parser)))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("invalid variable name"), parser->text, start, parser->pos - start);
        return Memmy_Status_ParseError;
    }

    U64 name_start = parser->pos;
    while (!Memmy_AddressParser_AtEnd(parser) && Memmy_VariableRef_IsIdentContinue(Memmy_AddressParser_Peek(parser)))
    {
        parser->pos++;
    }

    *out = (Memmy_VariableRef){
        .name = String8_Substr(parser->text, name_start, parser->pos - name_start),
    };
    return Memmy_Status_Ok;
}

static U32 Memmy_AddressParser_HexDigitValue(U8 c)
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

static Memmy_Status Memmy_AddressParser_ParseUnsigned(Memmy_AddressParser *parser, U64 limit, String8 message, U64 *out)
{
    U64 start = parser->pos;
    U32 base = 10;
    if (parser->pos + 1 < parser->text.len && parser->text.data[parser->pos] == '0' &&
        (parser->text.data[parser->pos + 1] == 'x' || parser->text.data[parser->pos + 1] == 'X'))
    {
        base = 16;
        parser->pos += 2;
        if (Memmy_AddressParser_AtEnd(parser) ||
            Memmy_AddressParser_HexDigitValue(Memmy_AddressParser_Peek(parser)) >= 16)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("expected hexadecimal digit"), parser->text, parser->pos, 1);
            return Memmy_Status_ParseError;
        }
    }
    else if (Memmy_AddressParser_AtEnd(parser) || !Char8_IsDigit(Memmy_AddressParser_Peek(parser)))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"), message, parser->text,
                                 parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    U64 value = 0;
    for (; parser->pos < parser->text.len; parser->pos++)
    {
        U8 c = parser->text.data[parser->pos];
        U32 digit = (base == 16) ? Memmy_AddressParser_HexDigitValue(c) : (Char8_IsDigit(c) ? (U32)(c - '0') : U32_MAX);
        if (digit >= base)
        {
            break;
        }

        if (value > (limit - digit) / base)
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_Overflow, String8_Lit("expr"),
                                     String8_Lit("integer overflow"), parser->text, parser->pos, 1);
            return Memmy_Status_Overflow;
        }
        value = value * base + digit;
    }

    if (parser->pos == start)
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"), message, parser->text,
                                 parser->pos, 1);
        return Memmy_Status_ParseError;
    }

    *out = value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressParser_ParseParenthesizedOffset(Memmy_AddressParser *parser, Memmy_ConstExpr *out)
{
    U64 open = parser->pos;
    U64 depth = 0;
    U64 close = STRING8_NPOS;
    for (; parser->pos < parser->text.len; parser->pos++)
    {
        U8 c = parser->text.data[parser->pos];
        if (c == '(')
        {
            depth++;
        }
        else if (c == ')')
        {
            depth--;
            if (depth == 0)
            {
                close = parser->pos;
                parser->pos++;
                break;
            }
        }
    }

    if (close == STRING8_NPOS)
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected ')'"), parser->text, parser->text.len, 0);
        return Memmy_Status_ParseError;
    }

    String8 inner = String8_Substr(parser->text, open + 1, close - open - 1);
    Memmy_ConstExpr const_expr = {0};
    Memmy_Status status = Memmy_ConstExpr_Parse(parser->arena, inner, &const_expr, parser->error);
    if (status != Memmy_Status_Ok)
    {
        if (parser->error != 0)
        {
            parser->error->input = parser->text;
            parser->error->byte_offset += open + 1;
        }
        return status;
    }

    *out = const_expr;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressParser_ParseOffset(Memmy_AddressParser *parser, Memmy_ConstExpr *out)
{
    if (Memmy_AddressParser_AtEnd(parser))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected offset"), parser->text, parser->pos, 0);
        return Memmy_Status_ParseError;
    }

    if (Memmy_AddressParser_Peek(parser) == '(')
    {
        return Memmy_AddressParser_ParseParenthesizedOffset(parser, out);
    }
    if (Memmy_AddressParser_Peek(parser) == '$')
    {
        Memmy_VariableRef variable = {0};
        Memmy_Status status = Memmy_AddressParser_ParseVariable(parser, &variable);
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

    U64 value = 0;
    Memmy_Status status =
        Memmy_AddressParser_ParseUnsigned(parser, (U64)I64_MAX, String8_Lit("expected offset"), &value);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out = (Memmy_ConstExpr){
        .kind = Memmy_ConstExprKind_Literal,
        .value = (I64)value,
    };
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressParser_ParseTargetBase(Memmy_AddressParser *parser, Memmy_AddressExpr *out)
{
    U64 close = String8_FindChar(parser->text, '>', parser->pos);
    if (close == STRING8_NPOS)
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected target ref"), parser->text, parser->pos, parser->text.len);
        return Memmy_Status_ParseError;
    }

    String8 target_text = String8_Substr(parser->text, parser->pos, close - parser->pos + 1);
    Memmy_TargetExpr target = {0};
    Memmy_Status status = Memmy_TargetExpr_Parse(target_text, &target, parser->error);
    if (status != Memmy_Status_Ok)
    {
        if (parser->error != 0)
        {
            parser->error->input = parser->text;
            parser->error->context = String8_Lit("expr");
        }
        return status;
    }
    parser->pos = close + 1;
    if (target.kind == Memmy_TargetExprKind_WholeProcess)
    {
        if (Memmy_AddressParser_AtEnd(parser) || !Char8_IsDigit(Memmy_AddressParser_Peek(parser)))
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("whole-process target is not a valid address base"), parser->text,
                                     parser->pos - target_text.len, target_text.len);
            return Memmy_Status_ParseError;
        }

        U64 addr = 0;
        status = Memmy_AddressParser_ParseUnsigned(parser, U64_MAX, String8_Lit("expected absolute address"), &addr);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        out->base_kind = Memmy_AddressExprBaseKind_ProcessAbsolute;
        out->target = target;
        out->absolute = addr;
        return Memmy_Status_Ok;
    }

    out->base_kind = Memmy_AddressExprBaseKind_Target;
    out->target = target;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressParser_ParseBase(Memmy_AddressParser *parser, Memmy_AddressExpr *out)
{
    if (Memmy_AddressParser_AtEnd(parser))
    {
        Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected address base"), parser->text, 0, 0);
        return Memmy_Status_ParseError;
    }

    if (Memmy_AddressParser_Peek(parser) == '<')
    {
        return Memmy_AddressParser_ParseTargetBase(parser, out);
    }
    if (Memmy_AddressParser_Peek(parser) == '$')
    {
        Memmy_VariableRef variable = {0};
        Memmy_Status status = Memmy_AddressParser_ParseVariable(parser, &variable);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        out->base_kind = Memmy_AddressExprBaseKind_Variable;
        out->variable = variable;
        return Memmy_Status_Ok;
    }

    U64 addr = 0;
    Memmy_Status status =
        Memmy_AddressParser_ParseUnsigned(parser, U64_MAX, String8_Lit("expected address base"), &addr);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    out->base_kind = Memmy_AddressExprBaseKind_Absolute;
    out->absolute = addr;
    return Memmy_Status_Ok;
}

static void Memmy_AddressParser_PushOp(Memmy_AddressParser *parser, Memmy_AddressExpr *expr, Memmy_AddressOpKind kind,
                                       Memmy_ConstExpr offset_expr)
{
    Memmy_AddressOp *op = Arena_PushStruct(parser->arena, Memmy_AddressOp);
    op->kind = kind;
    op->offset = offset_expr.value;
    op->offset_expr = offset_expr;
    List_PushBack(&expr->ops, &op->link);
}

static Memmy_Status Memmy_AddressParser_ParseOps(Memmy_AddressParser *parser, Memmy_AddressExpr *expr)
{
    while (!Memmy_AddressParser_AtEnd(parser))
    {
        U8 c = Memmy_AddressParser_Peek(parser);
        if (Memmy_AddressParser_IsWhitespace(c))
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("unexpected whitespace in address expression"), parser->text,
                                     parser->pos, 1);
            return Memmy_Status_ParseError;
        }
        if (c == '.' && parser->pos + 1 < parser->text.len && parser->text.data[parser->pos + 1] == '.')
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("address ranges are not supported"), parser->text, parser->pos, 2);
            return Memmy_Status_ParseError;
        }

        if (c == '+')
        {
            parser->pos++;
            Memmy_ConstExpr offset = {0};
            Memmy_Status status = Memmy_AddressParser_ParseOffset(parser, &offset);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            Memmy_AddressParser_PushOp(parser, expr, Memmy_AddressOpKind_Add, offset);
        }
        else if (c == '-')
        {
            if (parser->pos + 1 < parser->text.len && parser->text.data[parser->pos + 1] == '>')
            {
                parser->pos += 2;
                Memmy_ConstExpr offset = {0};
                if (Memmy_AddressParser_AtEnd(parser))
                {
                    Memmy_AddressParser_PushOp(parser, expr, Memmy_AddressOpKind_Deref, (Memmy_ConstExpr){0});
                    continue;
                }
                U8 next = Memmy_AddressParser_Peek(parser);
                if (next == '(' || next == '$' || Char8_IsDigit(next))
                {
                    Memmy_Status status = Memmy_AddressParser_ParseOffset(parser, &offset);
                    if (status != Memmy_Status_Ok)
                    {
                        return status;
                    }
                    Memmy_AddressParser_PushOp(parser, expr, Memmy_AddressOpKind_DerefOffset, offset);
                    continue;
                }

                Memmy_AddressParser_PushOp(parser, expr, Memmy_AddressOpKind_Deref, (Memmy_ConstExpr){0});
            }
            else
            {
                parser->pos++;
                Memmy_ConstExpr offset = {0};
                Memmy_Status status = Memmy_AddressParser_ParseOffset(parser, &offset);
                if (status != Memmy_Status_Ok)
                {
                    return status;
                }
                Memmy_AddressParser_PushOp(parser, expr, Memmy_AddressOpKind_Sub, offset);
            }
        }
        else
        {
            Memmy_ExprError_SetInput(parser->error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("unexpected input in address expression"), parser->text, parser->pos,
                                     1);
            return Memmy_Status_ParseError;
        }
    }

    return Memmy_Status_Ok;
}

Memmy_Status Memmy_AddressExpr_Parse(Arena *arena, String8 text, Memmy_AddressExpr *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing arena or output address expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_AddressParser parser = {
        .arena = arena,
        .text = text,
        .error = error,
    };

    Memmy_AddressExpr result = {0};
    Memmy_Status status = Memmy_AddressParser_ParseBase(&parser, &result);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    status = Memmy_AddressParser_ParseOps(&parser, &result);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out = result;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
