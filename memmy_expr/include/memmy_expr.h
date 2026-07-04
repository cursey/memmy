#ifndef MEMMY_EXPR_H
#define MEMMY_EXPR_H

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

Memmy_Status Memmy_TargetExpr_Parse(String8 text, Memmy_TargetExpr *out, Memmy_Error *error);
Memmy_Status Memmy_ConstExpr_Evaluate(String8 text, Memmy_ConstExpr *out, Memmy_Error *error);

#endif // MEMMY_EXPR_H
