#ifndef MEMMY_DSL_H
#define MEMMY_DSL_H

#include "base_string.h"
#include "memmy.h"

typedef U32 Memmy_ProcessSelectorKind;
enum
{
    Memmy_ProcessSelectorKind_None,
    Memmy_ProcessSelectorKind_Pid,
    Memmy_ProcessSelectorKind_Name,
};

typedef struct Memmy_ProcessSelector Memmy_ProcessSelector;
struct Memmy_ProcessSelector
{
    Memmy_ProcessSelectorKind kind;
    U32 pid;
    String8 name;
};

typedef U32 Memmy_TargetExprKind;
enum
{
    Memmy_TargetExprKind_Module,
    Memmy_TargetExprKind_WholeProcess,
};

typedef struct Memmy_TargetExpr Memmy_TargetExpr;
struct Memmy_TargetExpr
{
    Memmy_TargetExprKind kind;
    Memmy_ProcessSelector process;
    String8 module_name;
};

typedef struct Memmy_ConstExpr Memmy_ConstExpr;
struct Memmy_ConstExpr
{
    I64 value;
};

typedef U32 Memmy_AddressExprBaseKind;
enum
{
    Memmy_AddressExprBaseKind_Absolute,
    Memmy_AddressExprBaseKind_Target,
};

typedef U32 Memmy_AddressOpKind;
enum
{
    Memmy_AddressOpKind_Add,
    Memmy_AddressOpKind_Sub,
    Memmy_AddressOpKind_Deref,
    Memmy_AddressOpKind_DerefOffset,
};

typedef struct Memmy_AddressOp Memmy_AddressOp;
struct Memmy_AddressOp
{
    ListLink link;
    Memmy_AddressOpKind kind;
    I64 offset;
};

typedef struct Memmy_AddressExpr Memmy_AddressExpr;
struct Memmy_AddressExpr
{
    Memmy_AddressExprBaseKind base_kind;
    Memmy_Addr absolute;
    Memmy_TargetExpr target;
    List ops; // Memmy_AddressOp
};

typedef U32 Memmy_RangeExprKind;
enum
{
    Memmy_RangeExprKind_Target,
    Memmy_RangeExprKind_ModuleOffset,
    Memmy_RangeExprKind_ModuleSized,
    Memmy_RangeExprKind_AddressSized,
};

typedef struct Memmy_RangeExpr Memmy_RangeExpr;
struct Memmy_RangeExpr
{
    Memmy_RangeExprKind kind;
    Memmy_TargetExpr target;
    I64 start_offset;
    I64 end_offset;
    Memmy_Size size;
    Memmy_AddressExpr address;
};

typedef U32 Memmy_MemoryExprKind;
enum
{
    Memmy_MemoryExprKind_Address,
    Memmy_MemoryExprKind_Peek,
    Memmy_MemoryExprKind_Poke,
    Memmy_MemoryExprKind_PatternScan,
    Memmy_MemoryExprKind_ValueScan,
};

typedef struct Memmy_MemoryExpr Memmy_MemoryExpr;
struct Memmy_MemoryExpr
{
    Memmy_MemoryExprKind kind;
    Memmy_AddressExpr address;
    Memmy_RangeExpr range;
    Memmy_Type type;
    String8 value_text;
    String8 pattern_text;
    Memmy_Pattern pattern;
};

Memmy_Status Memmy_TargetExpr_Parse(String8 text, Memmy_TargetExpr *out, Memmy_Error *error);
Memmy_Status Memmy_ConstExpr_Evaluate(String8 text, Memmy_ConstExpr *out, Memmy_Error *error);
Memmy_Status Memmy_AddressExpr_Parse(Arena *arena, String8 text, Memmy_AddressExpr *out, Memmy_Error *error);
Memmy_Status Memmy_RangeExpr_Parse(Arena *arena, String8 text, Memmy_RangeExpr *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_Parse(Arena *arena, String8 text, Memmy_MemoryExpr *out, Memmy_Error *error);

#endif // MEMMY_DSL_H
