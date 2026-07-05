#ifndef MEMMY_EXEC_H
#define MEMMY_EXEC_H

#include "memmy.h"
#include "memmy_dsl.h"

typedef struct Memmy_ExecPeekResult Memmy_ExecPeekResult;
struct Memmy_ExecPeekResult
{
    Memmy_Addr address;
    Memmy_PointerWidth pointer_width;
    Memmy_Type type;
    Memmy_Value value;
};

typedef struct Memmy_ExecPokeResult Memmy_ExecPokeResult;
struct Memmy_ExecPokeResult
{
    U32 pid;
    Memmy_Addr address;
    Memmy_PointerWidth pointer_width;
    Memmy_Type type;
    Memmy_Value old_value;
    Memmy_Value new_value;
};

typedef U32 Memmy_ExecResultKind;
enum
{
    Memmy_ExecResultKind_Address,
    Memmy_ExecResultKind_Peek,
    Memmy_ExecResultKind_Poke,
    Memmy_ExecResultKind_Match,
    Memmy_ExecResultKind_Summary,
    Memmy_ExecResultKind_Process,
    Memmy_ExecResultKind_Assignment,
    Memmy_ExecResultKind_VariableBinding,
    Memmy_ExecResultKind_Unset,
    Memmy_ExecResultKind_Control,
};

typedef U32 Memmy_ExecControlKind;
enum
{
    Memmy_ExecControlKind_None,
    Memmy_ExecControlKind_Exit,
};

typedef struct Memmy_ExecAddressResult Memmy_ExecAddressResult;
struct Memmy_ExecAddressResult
{
    Memmy_Addr address;
    Memmy_PointerWidth pointer_width;
};

typedef struct Memmy_ExecMatchResult Memmy_ExecMatchResult;
struct Memmy_ExecMatchResult
{
    Memmy_Addr address;
    Memmy_PointerWidth pointer_width;
};

typedef struct Memmy_ExecSummaryResult Memmy_ExecSummaryResult;
struct Memmy_ExecSummaryResult
{
    U64 match_count;
};

typedef struct Memmy_ExecProcessResult Memmy_ExecProcessResult;
struct Memmy_ExecProcessResult
{
    Memmy_ProcessInfo info;
};

typedef struct Memmy_ExecAssignmentResult Memmy_ExecAssignmentResult;
struct Memmy_ExecAssignmentResult
{
    String8 name;
    Memmy_Status status;
};

typedef struct Memmy_ExecVariableBindingResult Memmy_ExecVariableBindingResult;
struct Memmy_ExecVariableBindingResult
{
    String8 name;
    Memmy_Status status;
};

typedef struct Memmy_ExecUnsetResult Memmy_ExecUnsetResult;
struct Memmy_ExecUnsetResult
{
    String8 name;
    Memmy_Status status;
};

typedef struct Memmy_ExecControlResult Memmy_ExecControlResult;
struct Memmy_ExecControlResult
{
    Memmy_ExecControlKind kind;
};

typedef struct Memmy_ExecResult Memmy_ExecResult;
struct Memmy_ExecResult
{
    Memmy_ExecResultKind kind;
    union {
        Memmy_ExecAddressResult address;
        Memmy_ExecPeekResult peek;
        Memmy_ExecPokeResult poke;
        Memmy_ExecMatchResult match;
        Memmy_ExecSummaryResult summary;
        Memmy_ExecProcessResult process;
        Memmy_ExecAssignmentResult assignment;
        Memmy_ExecVariableBindingResult variable_binding;
        Memmy_ExecUnsetResult unset;
        Memmy_ExecControlResult control;
    };
};

typedef struct Memmy_ExecResultSink Memmy_ExecResultSink;
struct Memmy_ExecResultSink
{
    Memmy_Status (*callback)(void *user_data, Memmy_ExecResult *result);
    void *user_data;
};

typedef struct Memmy_ExecProcessSelection Memmy_ExecProcessSelection;
struct Memmy_ExecProcessSelection
{
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
};

Memmy_Status Memmy_AddressExpr_Resolve(Memmy_Process *process, Memmy_AddressExpr *expr, Memmy_Addr *out,
                                       Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ResolveAddress(Memmy_Process *process, Memmy_MemoryExpr *expr, Memmy_Addr *out,
                                             Memmy_Error *error);
Memmy_Status Memmy_RangeExpr_Resolve(Memmy_Process *process, Memmy_RangeExpr *expr, Memmy_Range *out,
                                     Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePeek(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPeekResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePoke(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPokeResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePatternScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                                 Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecuteValueScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                               Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_Statement_Execute(Arena *arena, Memmy_Statement *statement, Memmy_ExecProcessSelection selection,
                                     Memmy_ExecResultSink sink, Memmy_Error *error);

#endif // MEMMY_EXEC_H
