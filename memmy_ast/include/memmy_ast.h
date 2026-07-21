#ifndef MEMMY_AST_H
#define MEMMY_AST_H

#include "base.h"

typedef U32 MemmyAst_Status;
enum
{
    MemmyAst_Status_Ok,
    MemmyAst_Status_InvalidArgument,
    MemmyAst_Status_ParseError,
    MemmyAst_Status_Overflow,
    MemmyAst_Status_Unsupported,
};

typedef U32 MemmyAst_NodeKind;
enum
{
    MemmyAst_NodeKind_Nil,
    MemmyAst_NodeKind_ConstArithmetic,
    MemmyAst_NodeKind_FloatLiteral,
    MemmyAst_NodeKind_StringLiteral,
    MemmyAst_NodeKind_Variable,
    MemmyAst_NodeKind_CurrentItem,
    MemmyAst_NodeKind_Target,
    MemmyAst_NodeKind_Address,
    MemmyAst_NodeKind_Range,
    // Specialized process-dependent syntax that evaluates to an ordinary range value.
    MemmyAst_NodeKind_ProcessRange,
    MemmyAst_NodeKind_Deref,
    MemmyAst_NodeKind_TypedRead,
    MemmyAst_NodeKind_PatternScan,
    MemmyAst_NodeKind_ValueScan,
    MemmyAst_NodeKind_Index,
    MemmyAst_NodeKind_ListTransform,
    MemmyAst_NodeKind_Assignment,
    MemmyAst_NodeKind_Command,
    MemmyAst_NodeKind_ReferenceScan,
    MemmyAst_NodeKind_DisasmScan,
    MemmyAst_NodeKind_Function,
    MemmyAst_NodeKind_ObjectBase,
    MemmyAst_NodeKind_ValuePipe,
};

typedef U32 MemmyAst_DisasmArch;
enum
{
    MemmyAst_DisasmArch_Null,
    MemmyAst_DisasmArch_X64,
};

typedef U32 MemmyAst_DisasmOperandKind;
enum
{
    MemmyAst_DisasmOperandKind_RegisterAny,
    MemmyAst_DisasmOperandKind_Register,
    MemmyAst_DisasmOperandKind_RipDisp32,
};

typedef struct MemmyAst_DisasmOperand MemmyAst_DisasmOperand;
struct MemmyAst_DisasmOperand
{
    MemmyAst_DisasmOperandKind kind;
    String8 reg;
};

typedef struct MemmyAst_DisasmInstruction MemmyAst_DisasmInstruction;
struct MemmyAst_DisasmInstruction
{
    String8 mnemonic;
    MemmyAst_DisasmOperand *operands;
    U32 operand_count;
};

typedef struct MemmyAst_DisasmPattern MemmyAst_DisasmPattern;
struct MemmyAst_DisasmPattern
{
    MemmyAst_DisasmArch arch;
    MemmyAst_DisasmInstruction *instructions;
    U32 instruction_count;
};

typedef U32 MemmyAst_ReferenceMode;
enum
{
    MemmyAst_ReferenceMode_Ptr,
    MemmyAst_ReferenceMode_Rel32,
    MemmyAst_ReferenceMode_Any,
};

typedef U32 MemmyAst_ConstOp;
enum
{
    MemmyAst_ConstOp_None,
    MemmyAst_ConstOp_Pos,
    MemmyAst_ConstOp_Neg,
    MemmyAst_ConstOp_Add,
    MemmyAst_ConstOp_Sub,
    MemmyAst_ConstOp_Mul,
    MemmyAst_ConstOp_Div,
    MemmyAst_ConstOp_Mod,
};

typedef U32 MemmyAst_CommandKind;
enum
{
    MemmyAst_CommandKind_None,
    MemmyAst_CommandKind_Procs,
    MemmyAst_CommandKind_Attach,
    MemmyAst_CommandKind_Detach,
    MemmyAst_CommandKind_Mods,
    MemmyAst_CommandKind_Regions,
    MemmyAst_CommandKind_Vars,
    MemmyAst_CommandKind_Unset,
    MemmyAst_CommandKind_Clear,
    MemmyAst_CommandKind_Help,
    MemmyAst_CommandKind_Tutorial,
    MemmyAst_CommandKind_Exit,
    MemmyAst_CommandKind_Quit,
};

typedef struct MemmyAst_Diagnostic MemmyAst_Diagnostic;
struct MemmyAst_Diagnostic
{
    String8 input;
    String8 message;
    String8 context;
    U64 byte_offset;
    U64 byte_count;
};

typedef struct MemmyAst_Node MemmyAst_Node;
struct MemmyAst_Node
{
    MemmyAst_NodeKind kind;
    String8 text;
    U64 byte_offset;
    U64 byte_count;
    I64 value;
    U64 floating_bits;
    String8 string;
    B32 contains_variable;
    MemmyAst_ConstOp op;
    MemmyAst_Node *lhs;
    MemmyAst_Node *rhs;
    MemmyAst_Node *value_expr;
    B32 range_is_sized;
    MemmyAst_ReferenceMode reference_mode;
    String8 name;
    String8 type_name;
    String8 pattern;
    MemmyAst_DisasmPattern disasm_pattern;
    String8 target_module;
};

typedef struct MemmyAst_Statement MemmyAst_Statement;
struct MemmyAst_Statement
{
    MemmyAst_NodeKind kind;
    MemmyAst_CommandKind command_kind;
    MemmyAst_Node *expr;
    MemmyAst_Node *assignment_value;
    String8 assignment_name;
    String8 command_arg;
    String8 text;
};

// Input text is copied into arena. All returned nodes and slices remain valid for the arena lifetime.
// Required outputs, and optional diagnostics when supplied, are cleared before validation.
MemmyAst_Status MemmyAst_Expr_Parse(Arena *arena, String8 text, MemmyAst_Node **out, MemmyAst_Diagnostic *diagnostic);
MemmyAst_Status MemmyAst_Statement_Parse(Arena *arena, String8 text, MemmyAst_Statement *out,
                                         MemmyAst_Diagnostic *diagnostic);

#endif // MEMMY_AST_H
