#include "memmy_ast_parser.h"

#include "base_checked.h"

static Memmy_AstStatus Memmy_Parser_ParseExpr(Memmy_Parser *parser, Memmy_AstNode **out);
static Memmy_AstStatus Memmy_Parser_ParseExprNoTransform(Memmy_Parser *parser, Memmy_AstNode **out);
static Memmy_AstStatus Memmy_Parser_ParseExprPrimary(Memmy_Parser *parser, Memmy_AstNode **out);
static Memmy_AstStatus Memmy_Parser_ParseTargetOrTargetAddress(Memmy_Parser *parser, Memmy_AstNode **out);
static B32 Memmy_Parser_TokenEndsExpr(Memmy_TokenKind kind);
static Memmy_AstStatus Memmy_Parser_ParseDerefChain(Memmy_Parser *parser, Memmy_AstNode **out);

static B32 Memmy_AstNode_CanFoldConst(Memmy_AstNode *node)
{
    B32 result = 0;
    if (node != 0 && node->kind == Memmy_AstNodeKind_ConstArithmetic && !node->contains_variable)
    {
        if (node->op == Memmy_AstConstOp_None)
        {
            result = 1;
        }
        else if (node->op == Memmy_AstConstOp_Pos || node->op == Memmy_AstConstOp_Neg)
        {
            result = Memmy_AstNode_CanFoldConst(node->lhs);
        }
        else
        {
            result = Memmy_AstNode_CanFoldConst(node->lhs) && Memmy_AstNode_CanFoldConst(node->rhs);
        }
    }
    return result;
}

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
        U32 digit = (base == 16) ? Char8_HexDigitValue(c) : (U32)(c - '0');
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
        if (Char8_IsWhitespace(c))
        {
            Memmy_Parser_SetError(parser, String8_Lit("invalid target"), token.byte_offset + 1 + i, 1);
            return Memmy_AstStatus_ParseError;
        }
        if (c == '!')
        {
            bang = i;
        }
    }

    if (bang != STRING8_NPOS)
    {
        Memmy_Parser_SetError(parser,
                              String8_Lit("process-qualified targets are not supported; use /attach or --pid/--name"),
                              token.byte_offset + 1 + bang, 1);
        return Memmy_AstStatus_ParseError;
    }

    node->target_module = body;
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

    if (parser->token.kind == Memmy_TokenKind_CurrentItem)
    {
        if (parser->current_item_scope_depth == 0)
        {
            Memmy_Parser_SetError(parser, String8_Lit("bare '$' is only valid inside transform expressions"),
                                  parser->token.byte_offset, parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }
        Memmy_Token token = parser->token;
        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_CurrentItem, token);
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
        status = Memmy_Parser_ParseExpr(parser, out);
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
        if (Memmy_AstNode_CanFoldConst(operand))
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

    if (Memmy_AstNode_CanFoldConst(lhs) && Memmy_AstNode_CanFoldConst(rhs))
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

Memmy_AstStatus Memmy_Parser_ParseConstSum(Memmy_Parser *parser, Memmy_AstNode **out)
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

static Memmy_AstStatus Memmy_Parser_ParseAbsoluteAddress(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_Token at = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *value_expr = 0;
    status = Memmy_Parser_ParseConstProduct(parser, &value_expr);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Address, at);
    node->value_expr = value_expr;
    node->contains_variable = value_expr->contains_variable;
    node->byte_count = value_expr->byte_offset + value_expr->byte_count - at.byte_offset;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
    *out = node;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseRangeEndpoint(Memmy_Parser *parser, Memmy_AstNode **out)
{
    return Memmy_Parser_ParseExprAdditive(parser, out);
}

static Memmy_AstStatus Memmy_Parser_ParseRange(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_Token open = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind == Memmy_TokenKind_Integer && parser->token.text.len == 1 &&
        parser->token.text.data[0] == '0')
    {
        status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        if (parser->token.kind == Memmy_TokenKind_DotDot)
        {
            status = Memmy_Parser_Next(parser);
            if (status != Memmy_AstStatus_Ok)
            {
                return status;
            }
            if (parser->token.kind == Memmy_TokenKind_RBracket)
            {
                Memmy_Token close = parser->token;
                Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ProcessRange, open);
                node->byte_count = close.byte_offset + close.byte_count - open.byte_offset;
                node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
                *out = node;
                return Memmy_Parser_Next(parser);
            }
        }
        Memmy_Parser_SetError(parser, String8_Lit("expected [0..] or address range"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    if (parser->token.kind != Memmy_TokenKind_At && parser->token.kind != Memmy_TokenKind_Target &&
        parser->token.kind != Memmy_TokenKind_Variable && parser->token.kind != Memmy_TokenKind_CurrentItem &&
        parser->token.kind != Memmy_TokenKind_Integer && parser->token.kind != Memmy_TokenKind_Plus &&
        parser->token.kind != Memmy_TokenKind_Minus && parser->token.kind != Memmy_TokenKind_LParen)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected address expression or target"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_AstNode *start = 0;
    status = Memmy_Parser_ParseRangeEndpoint(parser, &start);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind != Memmy_TokenKind_DotDot)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected '..'"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    B32 range_is_sized = 0;
    Memmy_AstNode *end_or_size = 0;
    if (parser->token.kind == Memmy_TokenKind_At || parser->token.kind == Memmy_TokenKind_Target ||
        parser->token.kind == Memmy_TokenKind_Variable || parser->token.kind == Memmy_TokenKind_CurrentItem ||
        parser->token.kind == Memmy_TokenKind_Integer || parser->token.kind == Memmy_TokenKind_Minus ||
        parser->token.kind == Memmy_TokenKind_LParen)
    {
        status = Memmy_Parser_ParseRangeEndpoint(parser, &end_or_size);
    }
    else if (parser->token.kind == Memmy_TokenKind_Plus)
    {
        range_is_sized = 1;
        status = Memmy_Parser_Next(parser);
        if (status == Memmy_AstStatus_Ok)
        {
            status = Memmy_Parser_ParseConstSum(parser, &end_or_size);
        }
    }
    else
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected range end or size"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind != Memmy_TokenKind_RBracket)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected ']'"), parser->token.byte_offset, parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_Token close = parser->token;
    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Range, open);
    node->lhs = start;
    node->rhs = end_or_size;
    node->range_is_sized = range_is_sized;
    node->contains_variable = start->contains_variable || end_or_size->contains_variable;
    node->byte_count = close.byte_offset + close.byte_count - open.byte_offset;
    node->text = String8_Substr(parser->input, node->byte_offset, node->byte_count);
    *out = node;
    return Memmy_Parser_Next(parser);
}

static Memmy_AstStatus Memmy_Parser_ParseTargetOrTargetAddress(Memmy_Parser *parser, Memmy_AstNode **out)
{
    return Memmy_Parser_ParseTargetNode(parser, out);
}

static Memmy_AstStatus Memmy_Parser_ParseAddressUnary(Memmy_Parser *parser, Memmy_AstNodeKind kind, Memmy_Token keyword,
                                                      Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }
    if (Memmy_Parser_TokenEndsExpr(parser->token.kind))
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected address expression"), keyword.byte_offset,
                              keyword.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_AstNode *operand = 0;
    if (parser->token.kind == Memmy_TokenKind_LParen || parser->token.kind == Memmy_TokenKind_At ||
        parser->token.kind == Memmy_TokenKind_LBracket || parser->token.kind == Memmy_TokenKind_Target)
    {
        status = Memmy_Parser_ParseExprPrimary(parser, &operand);
    }
    else
    {
        status = Memmy_Parser_ParseConstProduct(parser, &operand);
    }
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParsePostfix(parser, &operand, 1);
    }
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParseDerefChain(parser, &operand);
    }
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, kind, keyword);
    node->lhs = operand;
    node->contains_variable = operand->contains_variable;
    Memmy_Parser_NodeCoverInput(parser, node, keyword.byte_offset, operand->byte_offset + operand->byte_count);
    *out = node;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseExprPrimary(Memmy_Parser *parser, Memmy_AstNode **out)
{
    if (Memmy_Token_IsIdentifier(parser->token, String8_Lit("function")))
    {
        return Memmy_Parser_ParseAddressUnary(parser, Memmy_AstNodeKind_Function, parser->token, out);
    }

    if (Memmy_Token_IsIdentifier(parser->token, String8_Lit("objectbase")))
    {
        return Memmy_Parser_ParseAddressUnary(parser, Memmy_AstNodeKind_ObjectBase, parser->token, out);
    }

    if (parser->token.kind == Memmy_TokenKind_LParen)
    {
        Memmy_Token open = parser->token;
        Memmy_AstStatus status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        status = Memmy_Parser_ParseExpr(parser, out);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        if (parser->token.kind != Memmy_TokenKind_RParen)
        {
            Memmy_Parser_SetError(parser, String8_Lit("expected ')'"), parser->token.byte_offset,
                                  parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }

        U64 end = parser->token.byte_offset + parser->token.byte_count;
        Memmy_Parser_NodeCoverInput(parser, *out, open.byte_offset, end);
        return Memmy_Parser_Next(parser);
    }

    if (parser->token.kind == Memmy_TokenKind_At)
    {
        return Memmy_Parser_ParseAbsoluteAddress(parser, out);
    }

    if (parser->token.kind == Memmy_TokenKind_LBracket)
    {
        return Memmy_Parser_ParseRange(parser, out);
    }

    if (parser->token.kind == Memmy_TokenKind_Target)
    {
        return Memmy_Parser_ParseTargetOrTargetAddress(parser, out);
    }

    return Memmy_Parser_ParseConstProduct(parser, out);
}

static B32 Memmy_Parser_TokenEndsExpr(Memmy_TokenKind kind)
{
    return kind == Memmy_TokenKind_Eof || kind == Memmy_TokenKind_RBracket || kind == Memmy_TokenKind_RParen ||
           kind == Memmy_TokenKind_Equal || kind == Memmy_TokenKind_EqualEqual || kind == Memmy_TokenKind_LBrace ||
           kind == Memmy_TokenKind_FatArrow;
}

static Memmy_AstStatus Memmy_Parser_ParseDerefChain(Memmy_Parser *parser, Memmy_AstNode **out)
{
    while (parser->token.kind == Memmy_TokenKind_Arrow)
    {
        if (!Memmy_AstNode_MayBeAddressLike(*out))
        {
            Memmy_Parser_SetError(parser, String8_Lit("expected address before dereference"), parser->token.byte_offset,
                                  parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }

        Memmy_Token arrow = parser->token;
        Memmy_AstStatus status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        Memmy_AstNode *offset = 0;
        if (parser->token.kind != Memmy_TokenKind_Arrow && parser->token.kind != Memmy_TokenKind_As &&
            !Memmy_Parser_TokenEndsExpr(parser->token.kind))
        {
            status = Memmy_Parser_ParseConstSum(parser, &offset);
            if (status != Memmy_AstStatus_Ok)
            {
                return status;
            }
        }

        Memmy_AstNode *base = *out;
        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Deref, arrow);
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

    return Memmy_AstStatus_Ok;
}

Memmy_AstStatus Memmy_Parser_ParseExprAdditive(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseExprPrimary(parser, out);
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParsePostfix(parser, out, 1);
    }
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
        status = Memmy_Parser_ParseExprPrimary(parser, &rhs);
        if (status == Memmy_AstStatus_Ok)
        {
            status = Memmy_Parser_ParsePostfix(parser, &rhs, 1);
        }
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

static Memmy_AstStatus Memmy_Parser_ParseExprNoTransform(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseExprAdditive(parser, out);
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParseDerefChain(parser, out);
    }
    if (status == Memmy_AstStatus_Ok)
    {
        status = Memmy_Parser_ParsePostfix(parser, out, 0);
    }
    return status;
}

static Memmy_AstStatus Memmy_Parser_ParseExpr(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseExprNoTransform(parser, out);
    while (status == Memmy_AstStatus_Ok && parser->token.kind == Memmy_TokenKind_FatArrow)
    {
        Memmy_Token op = parser->token;
        status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        Memmy_AstNode *rhs = 0;
        parser->current_item_scope_depth++;
        status = Memmy_Parser_ParseExprNoTransform(parser, &rhs);
        parser->current_item_scope_depth--;
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }

        Memmy_AstNode *lhs = *out;
        Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ListTransform, op);
        node->lhs = lhs;
        node->rhs = rhs;
        node->contains_variable = lhs->contains_variable || rhs->contains_variable;
        Memmy_Parser_NodeCoverInput(parser, node, lhs->byte_offset, rhs->byte_offset + rhs->byte_count);
        *out = node;
    }
    return status;
}

Memmy_AstStatus Memmy_Parser_ParseExprOnly(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstStatus status = Memmy_Parser_ParseExpr(parser, out);
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
