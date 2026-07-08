#include "memmy_eval.h"

#include "Zydis.h"

typedef struct Memmy_EvalDisasmX64Needle Memmy_EvalDisasmX64Needle;
typedef struct Memmy_EvalDisasmX64Operand Memmy_EvalDisasmX64Operand;
typedef struct Memmy_EvalDisasmX64Instruction Memmy_EvalDisasmX64Instruction;
typedef struct Memmy_EvalDisasmX64Pattern Memmy_EvalDisasmX64Pattern;

struct Memmy_EvalDisasmX64Operand
{
    Memmy_AstDisasmOperandKind kind;
    ZydisRegister reg;
};

struct Memmy_EvalDisasmX64Instruction
{
    ZydisMnemonic mnemonic;
    Memmy_EvalDisasmX64Operand *operands;
    U32 operand_count;
};

struct Memmy_EvalDisasmX64Pattern
{
    Memmy_EvalDisasmX64Instruction *instructions;
    U32 instruction_count;
};

struct Memmy_EvalDisasmX64Needle
{
    Memmy_EvalDisasmX64Pattern pattern;
    ZydisDecoder decoder;
};

static B32 Memmy_EvalDisasmX64_MnemonicFromString(String8 text, ZydisMnemonic *out)
{
    for (U32 mnemonic = 0; mnemonic <= ZYDIS_MNEMONIC_MAX_VALUE; mnemonic++)
    {
        const char *name = ZydisMnemonicGetString((ZydisMnemonic)mnemonic);
        if (name != 0 && String8_EqNoCase(text, String8_FromCStr((char *)name)))
        {
            *out = (ZydisMnemonic)mnemonic;
            return 1;
        }
    }
    return 0;
}

static B32 Memmy_EvalDisasmX64_RegisterFromString(String8 text, ZydisRegister *out)
{
    for (U32 reg = 0; reg <= ZYDIS_REGISTER_MAX_VALUE; reg++)
    {
        const char *name = ZydisRegisterGetString((ZydisRegister)reg);
        if (name != 0 && String8_EqNoCase(text, String8_FromCStr((char *)name)))
        {
            *out = (ZydisRegister)reg;
            return 1;
        }
    }
    return 0;
}

static B32 Memmy_EvalDisasmX64_OperandMatches(Memmy_EvalDisasmX64Operand *pattern, ZydisDecodedOperand *operand)
{
    if (pattern->kind == Memmy_AstDisasmOperandKind_RegisterAny)
    {
        return operand->type == ZYDIS_OPERAND_TYPE_REGISTER;
    }
    if (pattern->kind == Memmy_AstDisasmOperandKind_Register)
    {
        return operand->type == ZYDIS_OPERAND_TYPE_REGISTER && operand->reg.value == pattern->reg;
    }
    if (pattern->kind == Memmy_AstDisasmOperandKind_RipDisp32)
    {
        return operand->type == ZYDIS_OPERAND_TYPE_MEMORY && operand->mem.base == ZYDIS_REGISTER_RIP &&
               operand->mem.index == ZYDIS_REGISTER_NONE && operand->mem.disp.size == 32;
    }
    return 0;
}

static B32 Memmy_EvalDisasmX64_MatchesAt(void *user_data, Memmy_Addr address, U8 *bytes, U64 available)
{
    Unused(address);

    Memmy_EvalDisasmX64Needle *needle = (Memmy_EvalDisasmX64Needle *)user_data;
    U64 offset = 0;
    for (U32 i = 0; i < needle->pattern.instruction_count; i++)
    {
        if (offset >= available)
        {
            return 0;
        }

        ZydisDecodedInstruction instruction = {0};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {0};
        ZyanStatus decode_status =
            ZydisDecoderDecodeFull(&needle->decoder, bytes + offset, available - offset, &instruction, operands);
        if (!ZYAN_SUCCESS(decode_status))
        {
            return 0;
        }

        Memmy_EvalDisasmX64Instruction *pattern_instruction = needle->pattern.instructions + i;
        if (instruction.mnemonic != pattern_instruction->mnemonic)
        {
            return 0;
        }
        if (instruction.operand_count_visible != pattern_instruction->operand_count)
        {
            return 0;
        }

        for (U32 operand_index = 0; operand_index < pattern_instruction->operand_count; operand_index++)
        {
            if (operands[operand_index].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT ||
                !Memmy_EvalDisasmX64_OperandMatches(pattern_instruction->operands + operand_index,
                                                    operands + operand_index))
            {
                return 0;
            }
        }

        offset += instruction.length;
    }
    return 1;
}

static Memmy_Status Memmy_EvalDisasmX64_ResolvePattern(Arena *arena, Memmy_AstDisasmPattern ast_pattern,
                                                       Memmy_EvalDisasmX64Pattern *out, Memmy_Error *error)
{
    Memmy_EvalDisasmX64Instruction *instructions =
        Arena_PushArray(arena, Memmy_EvalDisasmX64Instruction, ast_pattern.instruction_count);

    for (U32 instruction_index = 0; instruction_index < ast_pattern.instruction_count; instruction_index++)
    {
        Memmy_AstDisasmInstruction *ast_instruction = ast_pattern.instructions + instruction_index;
        ZydisMnemonic mnemonic = ZYDIS_MNEMONIC_INVALID;
        if (!Memmy_EvalDisasmX64_MnemonicFromString(ast_instruction->mnemonic, &mnemonic))
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("disasm"), String8_Lit("unknown mnemonic"));
            return Memmy_Status_ParseError;
        }

        Memmy_EvalDisasmX64Operand *operands =
            Arena_PushArray(arena, Memmy_EvalDisasmX64Operand, ast_instruction->operand_count);
        for (U32 operand_index = 0; operand_index < ast_instruction->operand_count; operand_index++)
        {
            Memmy_AstDisasmOperand *ast_operand = ast_instruction->operands + operand_index;
            operands[operand_index].kind = ast_operand->kind;
            if (ast_operand->kind == Memmy_AstDisasmOperandKind_Register)
            {
                ZydisRegister reg = ZYDIS_REGISTER_NONE;
                if (!Memmy_EvalDisasmX64_RegisterFromString(ast_operand->reg, &reg))
                {
                    Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("disasm"),
                                    String8_Lit("unknown register"));
                    return Memmy_Status_ParseError;
                }
                operands[operand_index].reg = reg;
            }
        }

        instructions[instruction_index] = (Memmy_EvalDisasmX64Instruction){
            .mnemonic = mnemonic,
            .operands = operands,
            .operand_count = ast_instruction->operand_count,
        };
    }

    *out = (Memmy_EvalDisasmX64Pattern){
        .instructions = instructions,
        .instruction_count = ast_pattern.instruction_count,
    };
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_DisasmX64Scan(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                      Memmy_AstDisasmPattern pattern, Memmy_ScanSink sink, Memmy_Error *error)
{
    if (pattern.instruction_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("disasm"), String8_Lit("empty pattern"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalDisasmX64Pattern resolved = {0};
    Memmy_Status resolve_status = Memmy_EvalDisasmX64_ResolvePattern(arena, pattern, &resolved, error);
    if (resolve_status != Memmy_Status_Ok)
    {
        return resolve_status;
    }

    Memmy_EvalDisasmX64Needle needle = {
        .pattern = resolved,
    };
    ZyanStatus status = ZydisDecoderInit(&needle.decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    if (!ZYAN_SUCCESS(status))
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("disasm"),
                        String8_Lit("failed to initialize x64 decoder"));
        return Memmy_Status_Unsupported;
    }

    U64 max_size = 15 * resolved.instruction_count;
    return Memmy_Process_ScanCustom(arena, process, options, 1, max_size, Memmy_EvalDisasmX64_MatchesAt, &needle, sink,
                                    error);
}
