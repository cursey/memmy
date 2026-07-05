#ifndef MEMMY_EXEC_H
#define MEMMY_EXEC_H

#include "base_hashmap.h"
#include "memmy.h"
#include "memmy_dsl.h"

typedef struct Memmy_ExecVariableBinding Memmy_ExecVariableBinding;
typedef struct Memmy_ExecResolveFrame Memmy_ExecResolveFrame;

typedef struct Memmy_ExecEnv Memmy_ExecEnv;
struct Memmy_ExecEnv
{
    Arena *arena;
    HashMap bindings; // Memmy_ExecVariableBinding
    Memmy_ExecResolveFrame *resolve_stack;
};

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
    Memmy_VariableExprKind variable_kind;
    Memmy_Status status;
};

typedef struct Memmy_ExecVariableBindingResult Memmy_ExecVariableBindingResult;
struct Memmy_ExecVariableBindingResult
{
    String8 name;
    Memmy_VariableExprKind variable_kind;
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

Memmy_ExecEnv Memmy_ExecEnv_Create(Arena *arena);
Memmy_Status Memmy_ExecEnv_Set(Memmy_ExecEnv *env, String8 name, Memmy_VariableExpr *expr, Memmy_Error *error);
Memmy_Status Memmy_ExecEnv_Unset(Memmy_ExecEnv *env, String8 name, Memmy_Error *error);
Memmy_Status Memmy_ExecEnv_Find(Memmy_ExecEnv *env, String8 name, Memmy_ExecVariableBinding **out, Memmy_Error *error);
Memmy_ExecVariableBinding *Memmy_ExecEnv_First(Memmy_ExecEnv *env);
Memmy_ExecVariableBinding *Memmy_ExecEnv_Next(Memmy_ExecEnv *env, Memmy_ExecVariableBinding *binding);
String8 Memmy_ExecVariableBinding_Name(Memmy_ExecVariableBinding *binding);
Memmy_VariableExprKind Memmy_ExecVariableBinding_Kind(Memmy_ExecVariableBinding *binding);
Memmy_VariableExpr *Memmy_ExecVariableBinding_Expr(Memmy_ExecVariableBinding *binding);
Memmy_Status Memmy_ExecEnv_ResolvePush(Memmy_ExecEnv *env, String8 name, Memmy_Error *error);
void Memmy_ExecEnv_ResolvePop(Memmy_ExecEnv *env);
Memmy_Status Memmy_ConstExpr_Resolve(Memmy_ExecEnv *env, Memmy_Process *process, Memmy_ConstExpr *expr, I64 *out,
                                     Memmy_Error *error);
Memmy_Status Memmy_AddressExpr_Resolve(Memmy_Process *process, Memmy_AddressExpr *expr, Memmy_Addr *out,
                                       Memmy_Error *error);
Memmy_Status Memmy_AddressExpr_ResolveWithEnv(Memmy_ExecEnv *env, Memmy_Process *process, Memmy_AddressExpr *expr,
                                              Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ResolveAddress(Memmy_Process *process, Memmy_MemoryExpr *expr, Memmy_Addr *out,
                                             Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ResolveAddressWithEnv(Memmy_ExecEnv *env, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                                    Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_RangeExpr_Resolve(Memmy_Process *process, Memmy_RangeExpr *expr, Memmy_Range *out,
                                     Memmy_Error *error);
Memmy_Status Memmy_RangeExpr_ResolveWithEnv(Memmy_ExecEnv *env, Memmy_Process *process, Memmy_RangeExpr *expr,
                                            Memmy_Range *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePeek(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPeekResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePeekWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
                                                 Memmy_MemoryExpr *expr, Memmy_ExecPeekResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePoke(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPokeResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePokeWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
                                                 Memmy_MemoryExpr *expr, Memmy_ExecPokeResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePatternScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                                 Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePatternScanWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
                                                        Memmy_MemoryExpr *expr, Memmy_ScanSink sink,
                                                        Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecuteValueScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                               Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecuteValueScanWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
                                                      Memmy_MemoryExpr *expr, Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_Exec_ValueParseWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process, Memmy_Type type,
                                          String8 text, Memmy_Value *out, Memmy_Error *error);
Memmy_Status Memmy_Statement_Execute(Arena *arena, Memmy_Statement *statement, Memmy_ExecProcessSelection selection,
                                     Memmy_ExecResultSink sink, Memmy_Error *error);
Memmy_Status Memmy_Statement_ExecuteWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Statement *statement,
                                            Memmy_ExecProcessSelection selection, Memmy_ExecResultSink sink,
                                            Memmy_Error *error);

#endif // MEMMY_EXEC_H
