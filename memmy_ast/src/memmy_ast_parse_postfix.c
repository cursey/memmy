#include "memmy_ast_parser.h"

static Memmy_AstStatus Memmy_Parser_ParsePatternScan(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstNode *range = *out;
    Memmy_Token open = parser->token;
    U64 pattern_start = parser->pos;
    B32 terminated = 0;
    while (!Memmy_Parser_AtEnd(parser))
    {
        if (Memmy_Parser_Peek(parser) == '}')
        {
            terminated = 1;
            break;
        }
        parser->pos++;
    }

    if (!terminated)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unterminated pattern"), open.byte_offset, open.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    U64 pattern_end = parser->pos;
    parser->pos++;
    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_PatternScan, open);
    node->lhs = range;
    node->pattern = String8_TrimWhitespace(String8_Substr(parser->input, pattern_start, pattern_end - pattern_start));
    node->contains_variable = range->contains_variable;
    Memmy_Parser_NodeCoverInput(parser, node, range->byte_offset, parser->pos);
    *out = node;
    return Memmy_Parser_Next(parser);
}

static Memmy_AstStatus Memmy_Parser_CaptureValueText(Memmy_Parser *parser, String8 *out)
{
    U64 start = parser->token.byte_offset;
    U64 pos = start;
    B32 in_string = 0;
    while (pos < parser->input.len)
    {
        U8 c = parser->input.data[pos];
        if (in_string)
        {
            if (c == '\\' && pos + 1 < parser->input.len)
            {
                pos += 2;
                continue;
            }
            if (c == '"')
            {
                in_string = 0;
            }
        }
        else
        {
            if (c == '"')
            {
                in_string = 1;
            }
            else if (c == ')' || c == '[')
            {
                break;
            }
        }
        pos++;
    }

    String8 value_text = String8_TrimWhitespace(String8_Substr(parser->input, start, pos - start));
    if (value_text.len == 0)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected value"), start, 1);
        return Memmy_AstStatus_ParseError;
    }

    parser->pos = pos;
    *out = value_text;
    return Memmy_Parser_Next(parser);
}

static Memmy_AstStatus Memmy_Parser_ParseTypedMemory(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstNode *base = *out;
    Memmy_Token as = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind != Memmy_TokenKind_Identifier)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected type name"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_Token type = parser->token;
    status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNodeKind kind = Memmy_AstNodeKind_TypedRead;
    Memmy_Token op = as;
    String8 value_text = {0};
    if (parser->token.kind == Memmy_TokenKind_Equal)
    {
        kind = Memmy_AstNodeKind_TypedWrite;
        op = parser->token;
        status = Memmy_Parser_Next(parser);
        if (status == Memmy_AstStatus_Ok)
        {
            status = Memmy_Parser_CaptureValueText(parser, &value_text);
        }
    }
    else if (parser->token.kind == Memmy_TokenKind_EqualEqual)
    {
        kind = Memmy_AstNodeKind_ValueScan;
        op = parser->token;
        status = Memmy_Parser_Next(parser);
        if (status == Memmy_AstStatus_Ok)
        {
            status = Memmy_Parser_CaptureValueText(parser, &value_text);
        }
    }
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, kind, op);
    node->lhs = base;
    node->type_name = type.text;
    node->value_text = value_text;
    node->contains_variable = base->contains_variable;
    U64 end = (parser->token.kind == Memmy_TokenKind_Eof) ? parser->input.len : parser->token.byte_offset;
    Memmy_Parser_NodeCoverInput(parser, node, base->byte_offset, end);
    *out = node;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseReferenceScan(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstNode *range = *out;
    Memmy_Token refs = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstReferenceMode mode = 0;
    if (Memmy_Token_IsIdentifier(parser->token, String8_Lit("ptr")))
    {
        mode = Memmy_AstReferenceMode_Ptr;
    }
    else if (Memmy_Token_IsIdentifier(parser->token, String8_Lit("rel32")))
    {
        mode = Memmy_AstReferenceMode_Rel32;
    }
    else if (Memmy_Token_IsIdentifier(parser->token, String8_Lit("any")))
    {
        mode = Memmy_AstReferenceMode_Any;
    }
    else if (parser->token.kind == Memmy_TokenKind_Identifier)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unknown reference scan mode"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    else
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected reference scan mode"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *target = 0;
    status = Memmy_Parser_ParseExprAdditive(parser, &target);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }
    if (!Memmy_AstNode_MayBeAddressLike(target))
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected reference target address"), target->byte_offset,
                              target->byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_ReferenceScan, refs);
    node->lhs = range;
    node->rhs = target;
    node->reference_mode = mode;
    node->contains_variable = range->contains_variable || target->contains_variable;
    Memmy_Parser_NodeCoverInput(parser, node, range->byte_offset, target->byte_offset + target->byte_count);
    *out = node;
    return Memmy_AstStatus_Ok;
}

static Memmy_AstStatus Memmy_Parser_ParseDisasmScan(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstNode *range = *out;
    Memmy_Token disasm = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    if (parser->token.kind != Memmy_TokenKind_Identifier)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected disasm architecture"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    if (!Memmy_Token_IsIdentifier(parser->token, String8_Lit("x64")))
    {
        Memmy_Parser_SetError(parser, String8_Lit("unsupported disasm architecture"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }
    if (parser->token.kind != Memmy_TokenKind_LBrace)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected disasm pattern body"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_Token open = parser->token;
    U64 body_start = parser->pos;
    B32 terminated = 0;
    while (!Memmy_Parser_AtEnd(parser))
    {
        if (Memmy_Parser_Peek(parser) == '}')
        {
            terminated = 1;
            break;
        }
        parser->pos++;
    }
    if (!terminated)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unterminated disasm pattern"), open.byte_offset, open.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    U64 body_end = parser->pos;
    parser->pos++;

    Memmy_AstDisasmPattern pattern = {0};
    String8 body = String8_Substr(parser->input, body_start, body_end - body_start);
    status =
        Memmy_Ast_ParseDisasmX64Pattern(parser->arena, parser->input, body, body_start, &pattern, parser->diagnostic);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_DisasmScan, disasm);
    node->lhs = range;
    node->disasm_pattern = pattern;
    node->contains_variable = range->contains_variable;
    Memmy_Parser_NodeCoverInput(parser, node, range->byte_offset, parser->pos);
    *out = node;
    return Memmy_Parser_Next(parser);
}

static Memmy_AstStatus Memmy_Parser_ParseIndex(Memmy_Parser *parser, Memmy_AstNode **out)
{
    Memmy_AstNode *base = *out;
    Memmy_Token open = parser->token;
    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    Memmy_AstNode *index = 0;
    status = Memmy_Parser_ParseConstSum(parser, &index);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }
    if (parser->token.kind != Memmy_TokenKind_RBracket)
    {
        Memmy_Parser_SetError(parser, String8_Lit("expected ']'"), parser->token.byte_offset, parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    U64 end = parser->token.byte_offset + parser->token.byte_count;
    Memmy_AstNode *node = Memmy_Parser_PushNode(parser, Memmy_AstNodeKind_Index, open);
    node->lhs = base;
    node->rhs = index;
    node->contains_variable = base->contains_variable || index->contains_variable;
    Memmy_Parser_NodeCoverInput(parser, node, base->byte_offset, end);
    *out = node;
    return Memmy_Parser_Next(parser);
}

Memmy_AstStatus Memmy_Parser_ParsePostfix(Memmy_Parser *parser, Memmy_AstNode **out, B32 tight_only)
{
    for (;;)
    {
        Memmy_AstStatus status = Memmy_AstStatus_Ok;
        if (parser->token.kind == Memmy_TokenKind_LBrace)
        {
            status = Memmy_Parser_ParsePatternScan(parser, out);
        }
        else if (!tight_only && parser->token.kind == Memmy_TokenKind_As)
        {
            status = Memmy_Parser_ParseTypedMemory(parser, out);
        }
        else if (!tight_only && Memmy_Token_IsIdentifier(parser->token, String8_Lit("refs")))
        {
            status = Memmy_Parser_ParseReferenceScan(parser, out);
        }
        else if (!tight_only && Memmy_Token_IsIdentifier(parser->token, String8_Lit("disasm")))
        {
            status = Memmy_Parser_ParseDisasmScan(parser, out);
        }
        else if (parser->token.kind == Memmy_TokenKind_LBracket)
        {
            status = Memmy_Parser_ParseIndex(parser, out);
        }
        else
        {
            break;
        }
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
    }

    return Memmy_AstStatus_Ok;
}
