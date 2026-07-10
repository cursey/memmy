#include "memmy_ast_parser.h"

typedef struct MemmyAst_DisasmParser MemmyAst_DisasmParser;
struct MemmyAst_DisasmParser
{
    String8 input;
    String8 body;
    U64 body_offset;
    U64 pos;
    MemmyAst_Diagnostic *diagnostic;
};

static B32 MemmyAst_DisasmParser_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static B32 MemmyAst_DisasmParser_IsIdentStart(U8 c)
{
    return Char8_IsAlpha(c) || c == '_';
}

static B32 MemmyAst_DisasmParser_IsIdentContinue(U8 c)
{
    return MemmyAst_DisasmParser_IsIdentStart(c) || Char8_IsDigit(c);
}

static void MemmyAst_DisasmParser_SetError(MemmyAst_DisasmParser *parser, String8 message, U64 offset, U64 count)
{
    if (parser->diagnostic != 0)
    {
        *parser->diagnostic = (MemmyAst_Diagnostic){
            .input = parser->input,
            .message = message,
            .context = String8_Lit("ast"),
            .byte_offset = parser->body_offset + offset,
            .byte_count = count,
        };
    }
}

static void MemmyAst_DisasmParser_SkipWhitespace(MemmyAst_DisasmParser *parser)
{
    while (parser->pos < parser->body.len && MemmyAst_DisasmParser_IsWhitespace(parser->body.data[parser->pos]))
    {
        parser->pos++;
    }
}

static String8 MemmyAst_DisasmParser_ParseIdent(MemmyAst_DisasmParser *parser)
{
    MemmyAst_DisasmParser_SkipWhitespace(parser);
    U64 start = parser->pos;
    if (start < parser->body.len && MemmyAst_DisasmParser_IsIdentStart(parser->body.data[start]))
    {
        parser->pos++;
        while (parser->pos < parser->body.len && MemmyAst_DisasmParser_IsIdentContinue(parser->body.data[parser->pos]))
        {
            parser->pos++;
        }
    }
    return String8_Substr(parser->body, start, parser->pos - start);
}

static B32 MemmyAst_DisasmParser_Consume(MemmyAst_DisasmParser *parser, U8 c)
{
    MemmyAst_DisasmParser_SkipWhitespace(parser);
    B32 result = parser->pos < parser->body.len && parser->body.data[parser->pos] == c;
    if (result)
    {
        parser->pos++;
    }
    return result;
}

static MemmyAst_Status MemmyAst_DisasmParser_ParseOperand(MemmyAst_DisasmParser *parser, MemmyAst_DisasmOperand *out)
{
    MemmyAst_DisasmParser_SkipWhitespace(parser);
    U64 start = parser->pos;
    if (MemmyAst_DisasmParser_Consume(parser, '['))
    {
        String8 base = MemmyAst_DisasmParser_ParseIdent(parser);
        if (!String8_EqNoCase(base, String8_Lit("rip")) || !MemmyAst_DisasmParser_Consume(parser, '+'))
        {
            MemmyAst_DisasmParser_SetError(parser, String8_Lit("expected [rip+disp32]"), start, 1);
            return MemmyAst_Status_ParseError;
        }
        String8 disp = MemmyAst_DisasmParser_ParseIdent(parser);
        if (!String8_EqNoCase(disp, String8_Lit("disp32")) || !MemmyAst_DisasmParser_Consume(parser, ']'))
        {
            MemmyAst_DisasmParser_SetError(parser, String8_Lit("expected [rip+disp32]"), start, parser->pos - start);
            return MemmyAst_Status_ParseError;
        }
        out->kind = MemmyAst_DisasmOperandKind_RipDisp32;
        return MemmyAst_Status_Ok;
    }

    String8 ident = MemmyAst_DisasmParser_ParseIdent(parser);
    if (ident.len == 0)
    {
        MemmyAst_DisasmParser_SetError(parser, String8_Lit("expected disasm operand"), start, 1);
        return MemmyAst_Status_ParseError;
    }
    if (String8_EqNoCase(ident, String8_Lit("reg")))
    {
        out->kind = MemmyAst_DisasmOperandKind_RegisterAny;
        return MemmyAst_Status_Ok;
    }

    out->kind = MemmyAst_DisasmOperandKind_Register;
    out->reg = ident;
    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_DisasmX64Pattern_Parse(Arena *arena, String8 input, String8 body, U64 body_offset,
                                                MemmyAst_DisasmPattern *out, MemmyAst_Diagnostic *diagnostic)
{
    if (arena == 0 || out == 0)
    {
        return MemmyAst_Status_InvalidArgument;
    }

    MemmyAst_DisasmParser parser = {
        .input = input,
        .body = body,
        .body_offset = body_offset,
        .diagnostic = diagnostic,
    };

    MemmyAst_DisasmInstruction *instructions =
        Arena_PushArray(arena, MemmyAst_DisasmInstruction, body.len == 0 ? 1 : body.len);
    U32 instruction_count = 0;

    for (;;)
    {
        MemmyAst_DisasmParser_SkipWhitespace(&parser);
        if (parser.pos >= body.len)
        {
            break;
        }
        if (body.data[parser.pos] == ';')
        {
            MemmyAst_DisasmParser_SetError(&parser, String8_Lit("expected disasm instruction"), parser.pos, 1);
            return MemmyAst_Status_ParseError;
        }

        String8 mnemonic_text = MemmyAst_DisasmParser_ParseIdent(&parser);
        if (mnemonic_text.len == 0)
        {
            MemmyAst_DisasmParser_SetError(&parser, String8_Lit("expected disasm instruction"), parser.pos, 1);
            return MemmyAst_Status_ParseError;
        }

        MemmyAst_DisasmOperand *operands = Arena_PushArray(arena, MemmyAst_DisasmOperand, 16);
        U32 operand_count = 0;
        MemmyAst_DisasmParser_SkipWhitespace(&parser);
        if (parser.pos < body.len && body.data[parser.pos] != ';')
        {
            for (;;)
            {
                if (operand_count >= 16)
                {
                    MemmyAst_DisasmParser_SetError(&parser, String8_Lit("too many disasm operands"), parser.pos, 1);
                    return MemmyAst_Status_ParseError;
                }
                MemmyAst_Status status = MemmyAst_DisasmParser_ParseOperand(&parser, operands + operand_count);
                if (status != MemmyAst_Status_Ok)
                {
                    return status;
                }
                operand_count++;
                if (!MemmyAst_DisasmParser_Consume(&parser, ','))
                {
                    break;
                }
            }
        }

        MemmyAst_DisasmParser_SkipWhitespace(&parser);
        if (parser.pos < body.len && body.data[parser.pos] != ';')
        {
            MemmyAst_DisasmParser_SetError(&parser, String8_Lit("expected ';'"), parser.pos, 1);
            return MemmyAst_Status_ParseError;
        }
        if (parser.pos < body.len)
        {
            parser.pos++;
        }

        instructions[instruction_count++] = (MemmyAst_DisasmInstruction){
            .mnemonic = mnemonic_text,
            .operands = operands,
            .operand_count = operand_count,
        };
    }

    if (instruction_count == 0)
    {
        MemmyAst_DisasmParser_SetError(&parser, String8_Lit("empty disasm pattern"), 0, body.len == 0 ? 1 : body.len);
        return MemmyAst_Status_ParseError;
    }

    *out = (MemmyAst_DisasmPattern){
        .arch = MemmyAst_DisasmArch_X64,
        .instructions = instructions,
        .instruction_count = instruction_count,
    };
    return MemmyAst_Status_Ok;
}
