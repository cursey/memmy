#ifndef MEMMY_EVAL_H
#define MEMMY_EVAL_H

#include "memmy.h"
#include "memmy_ast.h"

typedef U32 Memmy_EvalValueKind;
enum
{
    Memmy_EvalValueKind_Null,
    Memmy_EvalValueKind_Const,
    Memmy_EvalValueKind_Address,
    Memmy_EvalValueKind_Range,
    Memmy_EvalValueKind_ProcessRange,
    Memmy_EvalValueKind_AddressList,
    Memmy_EvalValueKind_RangeList,
    Memmy_EvalValueKind_TypedValue,
};

typedef struct Memmy_EvalEnv Memmy_EvalEnv;

typedef struct Memmy_EvalValue Memmy_EvalValue;
struct Memmy_EvalValue
{
    Memmy_EvalValueKind kind;
    I64 constant;
    Memmy_Addr address;
    Memmy_Range range;
    Memmy_Addr *addresses;
    U64 address_count;
    Memmy_Range *ranges;
    U64 range_count;
    Memmy_Value typed_value;
    Memmy_Value old_typed_value;
};

typedef U32 Memmy_EvalResultKind;
enum
{
    Memmy_EvalResultKind_Null,
    Memmy_EvalResultKind_Value,
    Memmy_EvalResultKind_Read,
    Memmy_EvalResultKind_Write,
    Memmy_EvalResultKind_AddressList,
    Memmy_EvalResultKind_Process,
    Memmy_EvalResultKind_Module,
    Memmy_EvalResultKind_Region,
    Memmy_EvalResultKind_Variable,
    Memmy_EvalResultKind_Unset,
    Memmy_EvalResultKind_Clear,
    Memmy_EvalResultKind_Help,
    Memmy_EvalResultKind_Exit,
};

typedef struct Memmy_EvalVariableResult Memmy_EvalVariableResult;
struct Memmy_EvalVariableResult
{
    String8 name;
    Memmy_EvalValue value;
};

typedef struct Memmy_EvalResult Memmy_EvalResult;
struct Memmy_EvalResult
{
    Memmy_EvalResultKind kind;
    Memmy_EvalValue value;
    Memmy_Addr address;
    Memmy_Value old_value;
    Memmy_Value new_value;
    Memmy_ProcessInfo process;
    Memmy_Module module;
    Memmy_Region region;
    Memmy_EvalVariableResult variable;
    String8 name;
    String8 text;
};

typedef Memmy_Status Memmy_EvalResultSinkFn(void *user_data, Memmy_EvalResult const *result);
typedef struct Memmy_EvalResultSink Memmy_EvalResultSink;
struct Memmy_EvalResultSink
{
    Memmy_EvalResultSinkFn *callback;
    void *user_data;
};

// The environment and all assigned bindings belong to arena. Values passed to Set are deep-copied.
Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena);
void Memmy_EvalEnv_SetDefaultProcess(Memmy_EvalEnv *env, U32 pid, Memmy_PointerWidth pointer_width);
void Memmy_EvalEnv_ClearDefaultProcess(Memmy_EvalEnv *env);
B32 Memmy_EvalEnv_GetDefaultProcess(Memmy_EvalEnv const *env, U32 *out_pid, Memmy_PointerWidth *out_pointer_width);
// Evaluation output data belongs to out_arena. AST inputs are borrowed and not modified.
// Sink result pointers and enumeration metadata are valid only for the callback duration.
Memmy_Status Memmy_EvalStatement(Arena *out_arena, Memmy_EvalEnv *env, Memmy_AstStatement const *statement,
                                 Memmy_EvalResultSink const *sink, Memmy_Error *error);
Memmy_Status Memmy_EvalExpr(Arena *out_arena, Memmy_EvalEnv *env, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                            Memmy_Error *error);
Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value);
// Find deep-copies the binding value into out_arena and clears out on failure.
Memmy_Status Memmy_EvalEnv_Find(Arena *out_arena, Memmy_EvalEnv const *env, String8 name, Memmy_EvalValue *out);
Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name);
void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env);

#endif // MEMMY_EVAL_H
