#include "memmy_ast_parser.h"

static MemmyAst_Status MemmyAst_Parser_ParsePatternScan(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Node *range = *out;
    MemmyAst_Token open = parser->token;
    U64 pattern_start = parser->pos;
    B32 terminated = 0;
    while (!MemmyAst_Parser_AtEnd(parser))
    {
        if (MemmyAst_Parser_Peek(parser) == '}')
        {
            terminated = 1;
            break;
        }
        parser->pos++;
    }

    if (!terminated)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unterminated pattern"), open.byte_offset, open.byte_count);
        return MemmyAst_Status_ParseError;
    }

    U64 pattern_end = parser->pos;
    parser->pos++;
    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_PatternScan, open);
    node->lhs = range;
    node->pattern = String8_TrimWhitespace(String8_Substr(parser->input, pattern_start, pattern_end - pattern_start));
    node->contains_variable = range->contains_variable;
    MemmyAst_Parser_NodeCoverInput(parser, node, range->byte_offset, parser->pos);
    *out = node;
    return MemmyAst_Parser_Next(parser);
}

static MemmyAst_Status MemmyAst_Parser_CaptureValueText(MemmyAst_Parser *parser, String8 *out)
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
        MemmyAst_Parser_SetError(parser, String8_Lit("expected value"), start, 1);
        return MemmyAst_Status_ParseError;
    }

    parser->pos = pos;
    *out = value_text;
    return MemmyAst_Parser_Next(parser);
}

static MemmyAst_Status MemmyAst_Parser_ParseTypedMemory(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Node *base = *out;
    MemmyAst_Token as = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind != MemmyAst_TokenKind_Identifier)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected type name"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Token type = parser->token;
    status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_NodeKind kind = MemmyAst_NodeKind_TypedRead;
    MemmyAst_Token op = as;
    String8 value_text = {0};
    if (parser->token.kind == MemmyAst_TokenKind_Equal)
    {
        kind = MemmyAst_NodeKind_TypedWrite;
        op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status == MemmyAst_Status_Ok)
        {
            status = MemmyAst_Parser_CaptureValueText(parser, &value_text);
        }
    }
    else if (parser->token.kind == MemmyAst_TokenKind_EqualEqual)
    {
        kind = MemmyAst_NodeKind_ValueScan;
        op = parser->token;
        status = MemmyAst_Parser_Next(parser);
        if (status == MemmyAst_Status_Ok)
        {
            status = MemmyAst_Parser_CaptureValueText(parser, &value_text);
        }
    }
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, kind, op);
    node->lhs = base;
    node->type_name = type.text;
    node->value_text = value_text;
    node->contains_variable = base->contains_variable;
    U64 end = (parser->token.kind == MemmyAst_TokenKind_Eof) ? parser->input.len : parser->token.byte_offset;
    MemmyAst_Parser_NodeCoverInput(parser, node, base->byte_offset, end);
    *out = node;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseReferenceScan(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Node *range = *out;
    MemmyAst_Token refs = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_ReferenceMode mode = 0;
    if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("ptr")))
    {
        mode = MemmyAst_ReferenceMode_Ptr;
    }
    else if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("rel32")))
    {
        mode = MemmyAst_ReferenceMode_Rel32;
    }
    else if (MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("any")))
    {
        mode = MemmyAst_ReferenceMode_Any;
    }
    else if (parser->token.kind == MemmyAst_TokenKind_Identifier)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unknown reference scan mode"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    else
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected reference scan mode"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *target = 0;
    status = MemmyAst_Parser_ParseExprAdditive(parser, &target);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }
    if (!MemmyAst_Node_MayBeAddressLike(target))
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected reference target address"), target->byte_offset,
                                 target->byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_ReferenceScan, refs);
    node->lhs = range;
    node->rhs = target;
    node->reference_mode = mode;
    node->contains_variable = range->contains_variable || target->contains_variable;
    MemmyAst_Parser_NodeCoverInput(parser, node, range->byte_offset, target->byte_offset + target->byte_count);
    *out = node;
    return MemmyAst_Status_Ok;
}

static MemmyAst_Status MemmyAst_Parser_ParseDisasmScan(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Node *range = *out;
    MemmyAst_Token disasm = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    if (parser->token.kind != MemmyAst_TokenKind_Identifier)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected disasm architecture"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    if (!MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("x64")))
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unsupported disasm architecture"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }
    if (parser->token.kind != MemmyAst_TokenKind_LBrace)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("expected disasm pattern body"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Token open = parser->token;
    U64 body_start = parser->pos;
    B32 terminated = 0;
    while (!MemmyAst_Parser_AtEnd(parser))
    {
        if (MemmyAst_Parser_Peek(parser) == '}')
        {
            terminated = 1;
            break;
        }
        parser->pos++;
    }
    if (!terminated)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unterminated disasm pattern"), open.byte_offset, open.byte_count);
        return MemmyAst_Status_ParseError;
    }

    U64 body_end = parser->pos;
    parser->pos++;

    MemmyAst_DisasmPattern pattern = {0};
    String8 body = String8_Substr(parser->input, body_start, body_end - body_start);
    status =
        MemmyAst_DisasmX64Pattern_Parse(parser->arena, parser->input, body, body_start, &pattern, parser->diagnostic);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_DisasmScan, disasm);
    node->lhs = range;
    node->disasm_pattern = pattern;
    node->contains_variable = range->contains_variable;
    MemmyAst_Parser_NodeCoverInput(parser, node, range->byte_offset, parser->pos);
    *out = node;
    return MemmyAst_Parser_Next(parser);
}

static MemmyAst_Status MemmyAst_Parser_ParseIndex(MemmyAst_Parser *parser, MemmyAst_Node **out)
{
    MemmyAst_Node *base = *out;
    MemmyAst_Token open = parser->token;
    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    MemmyAst_Node *index = 0;
    status = MemmyAst_Parser_ParseConstSum(parser, &index);
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

    U64 end = parser->token.byte_offset + parser->token.byte_count;
    MemmyAst_Node *node = MemmyAst_Parser_PushNode(parser, MemmyAst_NodeKind_Index, open);
    node->lhs = base;
    node->rhs = index;
    node->contains_variable = base->contains_variable || index->contains_variable;
    MemmyAst_Parser_NodeCoverInput(parser, node, base->byte_offset, end);
    *out = node;
    return MemmyAst_Parser_Next(parser);
}

MemmyAst_Status MemmyAst_Parser_ParsePostfix(MemmyAst_Parser *parser, MemmyAst_Node **out, B32 tight_only)
{
    for (;;)
    {
        MemmyAst_Status status = MemmyAst_Status_Ok;
        if (parser->token.kind == MemmyAst_TokenKind_LBrace)
        {
            status = MemmyAst_Parser_ParsePatternScan(parser, out);
        }
        else if (!tight_only && parser->token.kind == MemmyAst_TokenKind_As)
        {
            status = MemmyAst_Parser_ParseTypedMemory(parser, out);
        }
        else if (!tight_only && MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("refs")))
        {
            status = MemmyAst_Parser_ParseReferenceScan(parser, out);
        }
        else if (!tight_only && MemmyAst_Token_IsIdentifier(parser->token, String8_Lit("disasm")))
        {
            status = MemmyAst_Parser_ParseDisasmScan(parser, out);
        }
        else if (parser->token.kind == MemmyAst_TokenKind_LBracket)
        {
            status = MemmyAst_Parser_ParseIndex(parser, out);
        }
        else
        {
            break;
        }
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
    }

    return MemmyAst_Status_Ok;
}
