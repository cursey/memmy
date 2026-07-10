#include "memmy_ast_parser.h"

typedef struct Memmy_DisasmParser Memmy_DisasmParser;
struct Memmy_DisasmParser
{
    String8 input;
    String8 body;
    U64 body_offset;
    U64 pos;
    Memmy_AstDiagnostic *diagnostic;
};

static B32 Memmy_Disasm_Whitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static B32 Memmy_Disasm_IdentStart(U8 c)
{
    return Char8_IsAlpha(c) || c == '_';
}

static B32 Memmy_Disasm_IdentContinue(U8 c)
{
    return Memmy_Disasm_IdentStart(c) || Char8_IsDigit(c);
}

static void Memmy_Disasm_SetError(Memmy_DisasmParser *parser, String8 message, U64 offset, U64 count)
{
    if (parser->diagnostic != 0)
    {
        *parser->diagnostic = (Memmy_AstDiagnostic){
            .input = parser->input,
            .message = message,
            .context = String8_Lit("ast"),
            .byte_offset = parser->body_offset + offset,
            .byte_count = count,
        };
    }
}

static void Memmy_Disasm_SkipWhitespace(Memmy_DisasmParser *parser)
{
    while (parser->pos < parser->body.len && Memmy_Disasm_Whitespace(parser->body.data[parser->pos]))
    {
        parser->pos++;
    }
}

static String8 Memmy_Disasm_ParseIdent(Memmy_DisasmParser *parser)
{
    Memmy_Disasm_SkipWhitespace(parser);
    U64 start = parser->pos;
    if (start < parser->body.len && Memmy_Disasm_IdentStart(parser->body.data[start]))
    {
        parser->pos++;
        while (parser->pos < parser->body.len && Memmy_Disasm_IdentContinue(parser->body.data[parser->pos]))
        {
            parser->pos++;
        }
    }
    return String8_Substr(parser->body, start, parser->pos - start);
}

static B32 Memmy_Disasm_Consume(Memmy_DisasmParser *parser, U8 c)
{
    Memmy_Disasm_SkipWhitespace(parser);
    B32 result = parser->pos < parser->body.len && parser->body.data[parser->pos] == c;
    if (result)
    {
        parser->pos++;
    }
    return result;
}

static Memmy_AstStatus Memmy_Disasm_ParseOperand(Memmy_DisasmParser *parser, Memmy_AstDisasmOperand *out)
{
    Memmy_Disasm_SkipWhitespace(parser);
    U64 start = parser->pos;
    if (Memmy_Disasm_Consume(parser, '['))
    {
        String8 base = Memmy_Disasm_ParseIdent(parser);
        if (!String8_EqNoCase(base, String8_Lit("rip")) || !Memmy_Disasm_Consume(parser, '+'))
        {
            Memmy_Disasm_SetError(parser, String8_Lit("expected [rip+disp32]"), start, 1);
            return Memmy_AstStatus_ParseError;
        }
        String8 disp = Memmy_Disasm_ParseIdent(parser);
        if (!String8_EqNoCase(disp, String8_Lit("disp32")) || !Memmy_Disasm_Consume(parser, ']'))
        {
            Memmy_Disasm_SetError(parser, String8_Lit("expected [rip+disp32]"), start, parser->pos - start);
            return Memmy_AstStatus_ParseError;
        }
        out->kind = Memmy_AstDisasmOperandKind_RipDisp32;
        return Memmy_AstStatus_Ok;
    }

    String8 ident = Memmy_Disasm_ParseIdent(parser);
    if (ident.len == 0)
    {
        Memmy_Disasm_SetError(parser, String8_Lit("expected disasm operand"), start, 1);
        return Memmy_AstStatus_ParseError;
    }
    if (String8_EqNoCase(ident, String8_Lit("reg")))
    {
        out->kind = Memmy_AstDisasmOperandKind_RegisterAny;
        return Memmy_AstStatus_Ok;
    }

    out->kind = Memmy_AstDisasmOperandKind_Register;
    out->reg = ident;
    return Memmy_AstStatus_Ok;
}

Memmy_AstStatus Memmy_Ast_ParseDisasmX64Pattern(Arena *arena, String8 input, String8 body, U64 body_offset,
                                                Memmy_AstDisasmPattern *out, Memmy_AstDiagnostic *diagnostic)
{
    if (arena == 0 || out == 0)
    {
        return Memmy_AstStatus_InvalidArgument;
    }

    Memmy_DisasmParser parser = {
        .input = input,
        .body = body,
        .body_offset = body_offset,
        .diagnostic = diagnostic,
    };

    Memmy_AstDisasmInstruction *instructions =
        Arena_PushArray(arena, Memmy_AstDisasmInstruction, body.len == 0 ? 1 : body.len);
    U32 instruction_count = 0;

    for (;;)
    {
        Memmy_Disasm_SkipWhitespace(&parser);
        if (parser.pos >= body.len)
        {
            break;
        }
        if (body.data[parser.pos] == ';')
        {
            Memmy_Disasm_SetError(&parser, String8_Lit("expected disasm instruction"), parser.pos, 1);
            return Memmy_AstStatus_ParseError;
        }

        String8 mnemonic_text = Memmy_Disasm_ParseIdent(&parser);
        if (mnemonic_text.len == 0)
        {
            Memmy_Disasm_SetError(&parser, String8_Lit("expected disasm instruction"), parser.pos, 1);
            return Memmy_AstStatus_ParseError;
        }

        Memmy_AstDisasmOperand *operands = Arena_PushArray(arena, Memmy_AstDisasmOperand, 16);
        U32 operand_count = 0;
        Memmy_Disasm_SkipWhitespace(&parser);
        if (parser.pos < body.len && body.data[parser.pos] != ';')
        {
            for (;;)
            {
                if (operand_count >= 16)
                {
                    Memmy_Disasm_SetError(&parser, String8_Lit("too many disasm operands"), parser.pos, 1);
                    return Memmy_AstStatus_ParseError;
                }
                Memmy_AstStatus status = Memmy_Disasm_ParseOperand(&parser, operands + operand_count);
                if (status != Memmy_AstStatus_Ok)
                {
                    return status;
                }
                operand_count++;
                if (!Memmy_Disasm_Consume(&parser, ','))
                {
                    break;
                }
            }
        }

        Memmy_Disasm_SkipWhitespace(&parser);
        if (parser.pos < body.len && body.data[parser.pos] != ';')
        {
            Memmy_Disasm_SetError(&parser, String8_Lit("expected ';'"), parser.pos, 1);
            return Memmy_AstStatus_ParseError;
        }
        if (parser.pos < body.len)
        {
            parser.pos++;
        }

        instructions[instruction_count++] = (Memmy_AstDisasmInstruction){
            .mnemonic = mnemonic_text,
            .operands = operands,
            .operand_count = operand_count,
        };
    }

    if (instruction_count == 0)
    {
        Memmy_Disasm_SetError(&parser, String8_Lit("empty disasm pattern"), 0, body.len == 0 ? 1 : body.len);
        return Memmy_AstStatus_ParseError;
    }

    *out = (Memmy_AstDisasmPattern){
        .arch = Memmy_AstDisasmArch_X64,
        .instructions = instructions,
        .instruction_count = instruction_count,
    };
    return Memmy_AstStatus_Ok;
}
