#include "memmy_ast_parser.h"

#include "base.h"

static MemmyAst_Status MemmyAst_Parser_ParseExpr(MemmyAst_Parser *parser, MemmyAst_Node **out);
static MemmyAst_Status MemmyAst_Parser_ParseExprPrimary(MemmyAst_Parser *parser, MemmyAst_Node **out);
static MemmyAst_Status MemmyAst_Parser_ParseTargetOrTargetAddress(MemmyAst_Parser *parser, MemmyAst_Node **out);
static B32 MemmyAst_Parser_TokenEndsExpr(MemmyAst_TokenKind kind);
static MemmyAst_Status MemmyAst_Parser_ParseDerefChain(MemmyAst_Parser *parser, MemmyAst_Node **out);

static MemmyAst_Status MemmyAst_Parser_ParseIntegerValue(MemmyAst_Parser *parser, MemmyAst_Token token, U64 limit,
                                                         I64 *out)
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
        U32 digit = (base == 16) ? Char8_HexDigitValue(c) : (U32)(c - '0');
        if (value > (limit - digit) / base)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("integer literal overflow"), token.byte_offset + pos, 1);
            return MemmyAst_Status_Overflow;
        }
        value = value * base + digit;
    }

    *out = (I64)value;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseTargetNode(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Token token = parser->token;
    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Target, token);
    String8 body = String8_Substr(token.text, 1, token.text.len - 2);
    if (body.len == 0)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("empty target"), token.byte_offset + 1, 1);
        return MemmyAst_Status_ParseError;
    }

    U64 bang = STRING8_NPOS;
    for (U64 i = 0; i < body.len; i++)
    {
        U8 c = body.data[i];
        if (Char8_IsWhitespace(c))
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("invalid target"), token.byte_offset + 1 + i, 1);
            return MemmyAst_Status_ParseError;
        }
        if (c == '!')
        {
            bang = i;
        }
    }

    if (bang != STRING8_NPOS)
    {
        MemmyAst_Parser_SetError(
            parser, String8_Lit("process-qualified targets are not supported; use /attach or --pid/--name"),
            token.byte_offset + 1 + bang, 1);
        return MemmyAst_Status_ParseError;
    }

    node->target_module = body;
    *out = node;
    return MemmyAst_Parser_Next(parser);
}

static MemmyAst_Status MemmyAst_Parser_ParseConstPrimary(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    if (parser->token.kind == MemmyAst_TokenKind_Integer)
    {
        MemmyAst_Token token = parser->token;
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ConstArithmetic, token);
        node->op = MemmyAst_ConstOp_None;
        MemmyAst_Status status = MemmyAst_Parser_ParseIntegerValue(parser, token, (U64)I64_MAX, &node->value);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        *out = node;
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_Float)
    {
        MemmyAst_Token token = parser->token;
        F64 value = 0;
        U64 error_offset = 0;
        if (String8_ParseF64(token.text, &value, &error_offset) != String8_ParseStatus_Ok)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("invalid floating-point literal"),
                                     token.byte_offset + error_offset, 1);
            return MemmyAst_Status_ParseError;
        }
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_FloatLiteral, token);
        Memory_Copy(&node->floating_bits, &value, sizeof(value));
        *out = node;
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_String)
    {
        MemmyAst_Token token = parser->token;
        U8 *decoded = Arena_PushArrayNoZero(parser->arena, U8, token.text.len - 2);
        U64 decoded_len = 0;
        for (U64 i = 1; i + 1 < token.text.len; i++)
        {
            U8 c = token.text.data[i];
            if (c == '\\')
            {
                i++;
                c = token.text.data[i];
                if (c == 'n')
                {
                    c = '\n';
                }
                else if (c == 'r')
                {
                    c = '\r';
                }
                else if (c == 't')
                {
                    c = '\t';
                }
                else if (c != '\\' && c != '"')
                {
                    MemmyAst_Parser_SetError(parser, String8_Lit("invalid string escape"), token.byte_offset + i - 1,
                                             2);
                    return MemmyAst_Status_ParseError;
                }
            }
            decoded[decoded_len++] = c;
        }
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_StringLiteral, token);
        node->string = String8_Make(decoded, decoded_len);
        *out = node;
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_Variable)
    {
        MemmyAst_Token token = parser->token;
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Variable, token);
        node->name = String8_Substr(token.text, 1, token.text.len - 1);
        node->contains_variable = 1;
        *out = node;
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_CurrentItem)
    {
        if (parser->current_item_scope_depth == 0)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("bare '$' is only valid inside flow expressions"),
                                     parser->token.byte_offset, parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }
        MemmyAst_Token token = parser->token;
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_CurrentItem, token);
        node->contains_variable = 1;
        *out = node;
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_LParen)
    {
        U64 start = parser->token.byte_offset;
        MemmyAst_Status status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        status = MemmyAst_Parser_ParseExpr(parser, out);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        if (parser->token.kind != MemmyAst_TokenKind_RParen)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("expected ')'"), parser->token.byte_offset, 1);
            return MemmyAst_Status_ParseError;
        }
        (*out)->byte_offset = start;
        (*out)->byte_count = parser->token.byte_offset + parser->token.byte_count - start;
        return MemmyAst_Parser_Next(parser);
    }

    MemmyAst_Parser_SetError(parser, String8_Lit("expected expression"), parser->token.byte_offset, 1);
    return MemmyAst_Status_ParseError;
}

static MemmyAst_Status MemmyAst_Parser_ParseConstUnary(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    if (parser->token.kind == MemmyAst_TokenKind_Plus || parser->token.kind == MemmyAst_TokenKind_Minus)
    {
        MemmyAst_Token token = parser->token;
        MemmyAst_Status status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        if (token.kind == MemmyAst_TokenKind_Minus && parser->token.kind == MemmyAst_TokenKind_Integer)
        {
            MemmyAst_Token literal = parser->token;
            MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ConstArithmetic, token);
            node->op = MemmyAst_ConstOp_None;
            node->text = String8_Substr(parser->input, token.byte_offset,
                                        literal.byte_offset + literal.byte_count - token.byte_offset);
            node->byte_count = node->text.len;
            status = MemmyAst_Parser_ParseIntegerValue(parser, literal, (U64)I64_MAX + 1ull, &node->value);
            if (status != MemmyAst_Status_Ok)
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
            return MemmyAst_Parser_Next(parser);
        }

        if (token.kind == MemmyAst_TokenKind_Minus && parser->token.kind == MemmyAst_TokenKind_Float)
        {
            MemmyAst_Token literal = parser->token;
            String8 text = String8_Substr(parser->input, token.byte_offset,
                                          literal.byte_offset + literal.byte_count - token.byte_offset);
            F64 value = 0;
            U64 error_offset = 0;
            if (String8_ParseF64(text, &value, &error_offset) != String8_ParseStatus_Ok)
            {
                MemmyAst_Parser_SetError(parser, String8_Lit("invalid floating-point literal"),
                                         token.byte_offset + error_offset, 1);
                return MemmyAst_Status_ParseError;
            }
            MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_FloatLiteral, token);
            MemmyAst_Parser_NodeCoverInput(parser, node, token.byte_offset, literal.byte_offset + literal.byte_count);
            Memory_Copy(&node->floating_bits, &value, sizeof(value));
            *out = node;
            return MemmyAst_Parser_Next(parser);
        }

        MemmyAst_Node *operand = 0;
        status = MemmyAst_Parser_ParseConstUnary(parser, &operand);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ConstArithmetic, token);
        node->op = (token.kind == MemmyAst_TokenKind_Plus) ? MemmyAst_ConstOp_Pos : MemmyAst_ConstOp_Neg;
        node->lhs = operand;
        node->contains_variable = operand->contains_variable;
        node->byte_count = operand->byte_offset + operand->byte_count - token.byte_offset;
        *out = node;
        return MemmyAst_Status_Ok;
    }

    return MemmyAst_Parser_ParseConstPrimary(parser, out);
}

static MemmyAst_Status MemmyAst_Parser_CombineConst(MemmyAst_Parser *parser, MemmyAst_Node **out,
                                                    MemmyAst_Token op_token, MemmyAst_Node *rhs)
{
    MemmyAst_Node *lhs = *out;
    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ConstArithmetic, op_token);
    node->lhs = lhs;
    node->rhs = rhs;
    node->byte_offset = lhs->byte_offset;
    node->byte_count = rhs->byte_offset + rhs->byte_count - lhs->byte_offset;
    node->contains_variable = lhs->contains_variable || rhs->contains_variable;

    switch (op_token.kind)
    {
    case MemmyAst_TokenKind_Plus:
        node->op = MemmyAst_ConstOp_Add;
        break;
    case MemmyAst_TokenKind_Minus:
        node->op = MemmyAst_ConstOp_Sub;
        break;
    case MemmyAst_TokenKind_Star:
        node->op = MemmyAst_ConstOp_Mul;
        break;
    case MemmyAst_TokenKind_Slash:
        node->op = MemmyAst_ConstOp_Div;
        break;
    case MemmyAst_TokenKind_Percent:
        node->op = MemmyAst_ConstOp_Mod;
        break;
    default:
        break;
    }

    *out = node;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseConstProduct(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseConstUnary(parser, out);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    while (parser->token.kind == MemmyAst_TokenKind_Star || parser->token.kind == MemmyAst_TokenKind_Slash ||
           parser->token.kind == MemmyAst_TokenKind_Percent)
    {
        MemmyAst_Token op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        MemmyAst_Node *rhs = 0;
        status = MemmyAst_Parser_ParseConstUnary(parser, &rhs);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        status = MemmyAst_Parser_CombineConst(parser, out, op, rhs);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
    }

    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_Parser_ParseConstSum(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseConstProduct(parser, out);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    while (parser->token.kind == MemmyAst_TokenKind_Plus || parser->token.kind == MemmyAst_TokenKind_Minus)
    {
        MemmyAst_Token op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        MemmyAst_Node *rhs = 0;
        status = MemmyAst_Parser_ParseConstProduct(parser, &rhs);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        status = MemmyAst_Parser_CombineConst(parser, out, op, rhs);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
    }

    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseAbsoluteAddress(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Token at = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *value_expr = 0;
    status = MemmyAst_Parser_ParseConstProduct(parser, &value_expr);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Address, at);
    node->value_expr = value_expr;
    node->contains_variable = value_expr->contains_variable;
    node->byte_count = value_expr->byte_offset + value_expr->byte_count - at.byte_offset;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
    *out = node;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseRangeEndpoint(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    return MemmyAst_Parser_ParseExprAdditive(parser, out);
}

static MemmyAst_Status MemmyAst_Parser_ParseRange(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Token open = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind == MemmyAst_TokenKind_Integer && parser->token.text.len == 1 &&
        parser->token.text.data[0] == '0')
    {
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        if (parser->token.kind == MemmyAst_TokenKind_DotDot)
        {
            status = MemmyAst_Parser_Next(parser);
            if (status != MemmyAst_Status_Ok)
            {
                return status;
            }
            if (parser->token.kind == MemmyAst_TokenKind_RBracket)
            {
                MemmyAst_Token close = parser->token;
                MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ProcessRange, open);
                node->byte_count = close.byte_offset + close.byte_count - open.byte_offset;
                node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
                *out = node;
                return MemmyAst_Parser_Next(parser);
            }
        }
        MemmyAst_Parser_SetError(parser, String8_Lit("expected [0..] or address range"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    if (parser->token.kind != MemmyAst_TokenKind_At && parser->token.kind != MemmyAst_TokenKind_Target &&
        parser->token.kind != MemmyAst_TokenKind_Variable && parser->token.kind != MemmyAst_TokenKind_CurrentItem &&
        parser->token.kind != MemmyAst_TokenKind_Integer && parser->token.kind != MemmyAst_TokenKind_Plus &&
        parser->token.kind != MemmyAst_TokenKind_Minus && parser->token.kind != MemmyAst_TokenKind_LParen)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected address expression or target"),
                                 parser->token.byte_offset, parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Node *start = 0;
    status = MemmyAst_Parser_ParseRangeEndpoint(parser, &start);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind != MemmyAst_TokenKind_DotDot)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected '..'"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    B32 range_is_sized = 0;
    MemmyAst_Node *end_or_size = 0;
    if (parser->token.kind == MemmyAst_TokenKind_At || parser->token.kind == MemmyAst_TokenKind_Target ||
        parser->token.kind == MemmyAst_TokenKind_Variable || parser->token.kind == MemmyAst_TokenKind_CurrentItem ||
        parser->token.kind == MemmyAst_TokenKind_Integer || parser->token.kind == MemmyAst_TokenKind_Minus ||
        parser->token.kind == MemmyAst_TokenKind_LParen)
    {
        status = MemmyAst_Parser_ParseRangeEndpoint(parser, &end_or_size);
    }
    else if (parser->token.kind == MemmyAst_TokenKind_Plus)
    {
        range_is_sized = 1;
        status = MemmyAst_Parser_Next(parser);
        if (status == MemmyAst_Status_Ok)
        {
            status = MemmyAst_Parser_ParseConstSum(parser, &end_or_size);
        }
    }
    else
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected range end or size"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind != MemmyAst_TokenKind_RBracket)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected ']'"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Token close = parser->token;
    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Range, open);
    node->lhs = start;
    node->rhs = end_or_size;
    node->range_is_sized = range_is_sized;
    node->contains_variable = start->contains_variable || end_or_size->contains_variable;
    node->byte_count = close.byte_offset + close.byte_count - open.byte_offset;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
    *out = node;
    return MemmyAst_Parser_Next(parser);
}

static MemmyAst_Status MemmyAst_Parser_ParseTargetOrTargetAddress(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    return MemmyAst_Parser_ParseTargetNode(parser, out);
}

static MemmyAst_Status MemmyAst_Parser_ParseAddressUnary(MemmyAst_Parser *parser, MemmyAst_NodeKind kind,
                                                         MemmyAst_Token keyword, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }
    if (MemmyAst_Parser_TokenEndsExpr(parser->token.kind))
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected address expression"), keyword.byte_offset,
                                 keyword.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Node *operand = 0;
    if (parser->token.kind == MemmyAst_TokenKind_LParen || parser->token.kind == MemmyAst_TokenKind_At ||
        parser->token.kind == MemmyAst_TokenKind_LBracket || parser->token.kind == MemmyAst_TokenKind_Target)
    {
        status = MemmyAst_Parser_ParseExprPrimary(parser, &operand);
    }
    else
    {
        status = MemmyAst_Parser_ParseConstProduct(parser, &operand);
    }
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParsePostfix(parser, &operand, 1);
    }
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParseDerefChain(parser, &operand);
    }
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, kind, keyword);
    node->lhs = operand;
    node->contains_variable = operand->contains_variable;
    MemmyAst_Parser_NodeCoverInput(parser, node, keyword.byte_offset, operand->byte_offset + operand->byte_count);
    *out = node;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseExprPrimary(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("nil")))
    {
        MemmyAst_Token token = parser->token;
        *out = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Nil, token);
        return MemmyAst_Parser_Next(parser);
    }

    if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("function")))
    {
        return MemmyAst_Parser_ParseAddressUnary(parser, MemmyAst_NodeKind_Function, parser->token, out);
    }

    if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("objectbase")))
    {
        return MemmyAst_Parser_ParseAddressUnary(parser, MemmyAst_NodeKind_ObjectBase, parser->token, out);
    }

    if (parser->token.kind == MemmyAst_TokenKind_LParen)
    {
        MemmyAst_Token open = parser->token;
        MemmyAst_Status status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        status = MemmyAst_Parser_ParseExpr(parser, out);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        if (parser->token.kind != MemmyAst_TokenKind_RParen)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("expected ')'"), parser->token.byte_offset,
                                     parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }

        U64 end = parser->token.byte_offset + parser->token.byte_count;
        MemmyAst_Parser_NodeCoverInput(parser, *out, open.byte_offset, end);
        return MemmyAst_Parser_Next(parser);
    }

    if (parser->token.kind == MemmyAst_TokenKind_At)
    {
        return MemmyAst_Parser_ParseAbsoluteAddress(parser, out);
    }

    if (parser->token.kind == MemmyAst_TokenKind_LBracket)
    {
        return MemmyAst_Parser_ParseRange(parser, out);
    }

    if (parser->token.kind == MemmyAst_TokenKind_Target)
    {
        return MemmyAst_Parser_ParseTargetOrTargetAddress(parser, out);
    }

    return MemmyAst_Parser_ParseConstProduct(parser, out);
}

static B32 MemmyAst_Parser_TokenEndsExpr(MemmyAst_TokenKind kind)
{
    return kind == MemmyAst_TokenKind_Eof || kind == MemmyAst_TokenKind_RBracket || kind == MemmyAst_TokenKind_RParen ||
           kind == MemmyAst_TokenKind_Equal || kind == MemmyAst_TokenKind_EqualEqual ||
           kind == MemmyAst_TokenKind_LBrace || kind == MemmyAst_TokenKind_FatArrow ||
           kind == MemmyAst_TokenKind_ValuePipe;
}

static MemmyAst_Status MemmyAst_Parser_ParseDerefChain(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    while (parser->token.kind == MemmyAst_TokenKind_Arrow)
    {
        if (!MemmyAst_Node_MayBeAddressLike(*out))
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("expected address before dereference"),
                                     parser->token.byte_offset, parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }

        MemmyAst_Token arrow = parser->token;
        MemmyAst_Status status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        MemmyAst_Node *offset = 0;
        if (parser->token.kind != MemmyAst_TokenKind_Arrow && parser->token.kind != MemmyAst_TokenKind_As &&
            !MemmyAst_Parser_TokenEndsExpr(parser->token.kind))
        {
            status = MemmyAst_Parser_ParseConstSum(parser, &offset);
            if (status != MemmyAst_Status_Ok)
            {
                return status;
            }
        }

        MemmyAst_Node *base = *out;
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Deref, arrow);
        node->lhs = base;
        node->rhs = offset;
        node->byte_offset = base->byte_offset;
        if (offset != 0)
        {
            node->byte_count = offset->byte_offset + offset->byte_count - base->byte_offset;
            node->contains_variable = base->contains_variable || offset->contains_variable;
        }
        else
        {
            node->byte_count = arrow.byte_offset + arrow.byte_count - base->byte_offset;
            node->contains_variable = base->contains_variable;
        }
        node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
        *out = node;
    }

    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_Parser_ParseExprAdditive(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseExprPrimary(parser, out);
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParsePostfix(parser, out, 1);
    }
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    while (parser->token.kind == MemmyAst_TokenKind_Plus || parser->token.kind == MemmyAst_TokenKind_Minus)
    {
        MemmyAst_Token op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        MemmyAst_Node *rhs = 0;
        status = MemmyAst_Parser_ParseExprPrimary(parser, &rhs);
        if (status == MemmyAst_Status_Ok)
        {
            status = MemmyAst_Parser_ParsePostfix(parser, &rhs, 1);
        }
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        status = MemmyAst_Parser_CombineConst(parser, out, op, rhs);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
    }

    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_Parser_ParseExprNoTransform(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseExprAdditive(parser, out);
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParseDerefChain(parser, out);
    }
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParsePostfix(parser, out, 0);
    }
    return status;
}

static MemmyAst_Status MemmyAst_Parser_ParseExpr(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseExprNoTransform(parser, out);
    while (status == MemmyAst_Status_Ok &&
           (parser->token.kind == MemmyAst_TokenKind_FatArrow || parser->token.kind == MemmyAst_TokenKind_ValuePipe))
    {
        MemmyAst_Token op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        MemmyAst_Node *rhs = 0;
        parser->current_item_scope_depth++;
        status = MemmyAst_Parser_ParseExprNoTransform(parser, &rhs);
        parser->current_item_scope_depth--;
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }

        MemmyAst_Node *lhs = *out;
        MemmyAst_NodeKind kind =
            op.kind == MemmyAst_TokenKind_FatArrow ? MemmyAst_NodeKind_ListTransform : MemmyAst_NodeKind_ValuePipe;
        MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, kind, op);
        node->lhs = lhs;
        node->rhs = rhs;
        node->contains_variable = lhs->contains_variable || rhs->contains_variable;
        MemmyAst_Parser_NodeCoverInput(parser, node, lhs->byte_offset, rhs->byte_offset + rhs->byte_count);
        *out = node;
    }
    return status;
}

MemmyAst_Status MemmyAst_Parser_ParseExprOnly(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Status status = MemmyAst_Parser_ParseExpr(parser, out);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind != MemmyAst_TokenKind_Eof)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_Expr_Parse(Arena *arena, String8 text, MemmyAst_Node **out, MemmyAst_Diagnostic *diagnostic)
{
    if (out != 0)
    {
        *out = 0;
    }
    if (diagnostic != 0)
    {
        *diagnostic = (MemmyAst_Diagnostic){0};
    }
    if (arena == 0 || out == 0)
    {
        MemmyAst_Diagnostic_Set(diagnostic, text, String8_Lit("ast"), String8_Lit("missing arena or output"), 0, 0);
        return MemmyAst_Status_InvalidArgument;
    }

    text = String8_Copy(arena, text);

    MemmyAst_Parser parser = {
        .arena = arena,
        .input = text,
        .diagnostic = diagnostic,
    };
    MemmyAst_Status status = MemmyAst_Parser_Next(&parser);
    if (status == MemmyAst_Status_Ok)
    {
        status = MemmyAst_Parser_ParseExprOnly(&parser, out);
    }
    if (status != MemmyAst_Status_Ok && out != 0)
    {
        *out = 0;
    }
    if (status == MemmyAst_Status_Ok && diagnostic != 0)
    {
        *diagnostic = (MemmyAst_Diagnostic){0};
    }
    return status;
}
