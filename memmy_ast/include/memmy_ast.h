#ifndef MEMMY_AST_H
#define MEMMY_AST_H

#include "base_arena.h"
#include "base_string.h"

typedef U32 Memmy_AstStatus;
enum
{
    Memmy_AstStatus_Ok,
    Memmy_AstStatus_InvalidArgument,
    Memmy_AstStatus_ParseError,
    Memmy_AstStatus_Overflow,
    Memmy_AstStatus_Unsupported,
};

typedef U32 Memmy_AstNodeKind;
enum
{
    Memmy_AstNodeKind_Null,
    Memmy_AstNodeKind_ConstArithmetic,
    Memmy_AstNodeKind_Variable,
    Memmy_AstNodeKind_CurrentItem,
    Memmy_AstNodeKind_Target,
    Memmy_AstNodeKind_Address,
    Memmy_AstNodeKind_Range,
    Memmy_AstNodeKind_ProcessRange,
    Memmy_AstNodeKind_Deref,
    Memmy_AstNodeKind_TypedRead,
    Memmy_AstNodeKind_TypedWrite,
    Memmy_AstNodeKind_PatternScan,
    Memmy_AstNodeKind_ValueScan,
    Memmy_AstNodeKind_Index,
    Memmy_AstNodeKind_ListTransform,
    Memmy_AstNodeKind_Assignment,
    Memmy_AstNodeKind_Command,
};

typedef U32 Memmy_AstConstOp;
enum
{
    Memmy_AstConstOp_None,
    Memmy_AstConstOp_Pos,
    Memmy_AstConstOp_Neg,
    Memmy_AstConstOp_Add,
    Memmy_AstConstOp_Sub,
    Memmy_AstConstOp_Mul,
    Memmy_AstConstOp_Div,
    Memmy_AstConstOp_Mod,
};

typedef U32 Memmy_AstCommandKind;
enum
{
    Memmy_AstCommandKind_None,
    Memmy_AstCommandKind_Procs,
    Memmy_AstCommandKind_Attach,
    Memmy_AstCommandKind_Detach,
    Memmy_AstCommandKind_Mods,
    Memmy_AstCommandKind_Regions,
    Memmy_AstCommandKind_Vars,
    Memmy_AstCommandKind_Unset,
    Memmy_AstCommandKind_Clear,
    Memmy_AstCommandKind_Help,
    Memmy_AstCommandKind_Exit,
    Memmy_AstCommandKind_Quit,
};

typedef struct Memmy_AstDiagnostic Memmy_AstDiagnostic;
struct Memmy_AstDiagnostic
{
    String8 input;
    String8 message;
    String8 context;
    U64 byte_offset;
    U64 byte_count;
};

typedef struct Memmy_AstNode Memmy_AstNode;
struct Memmy_AstNode
{
    Memmy_AstNodeKind kind;
    String8 text;
    U64 byte_offset;
    U64 byte_count;
    I64 value;
    B32 contains_variable;
    Memmy_AstConstOp op;
    Memmy_AstNode *lhs;
    Memmy_AstNode *rhs;
    Memmy_AstNode *value_expr;
    B32 range_is_sized;
    String8 name;
    String8 type_name;
    String8 pattern;
    String8 value_text;
    String8 target_module;
};

typedef struct Memmy_AstStatement Memmy_AstStatement;
struct Memmy_AstStatement
{
    Memmy_AstNodeKind kind;
    Memmy_AstCommandKind command_kind;
    Memmy_AstNode *expr;
    Memmy_AstNode *assignment_value;
    String8 assignment_name;
    String8 command_arg;
    String8 text;
};

Memmy_AstStatus Memmy_Ast_ParseExpr(Arena *arena, String8 text, Memmy_AstNode **out, Memmy_AstDiagnostic *diagnostic);
Memmy_AstStatus Memmy_Ast_ParseStatement(Arena *arena, String8 text, Memmy_AstStatement *out,
                                         Memmy_AstDiagnostic *diagnostic);

#endif // MEMMY_AST_H
