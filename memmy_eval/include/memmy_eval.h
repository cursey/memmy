#ifndef MEMMY_EVAL_H
#define MEMMY_EVAL_H

#include "base_hashmap.h"
#include "memmy.h"
#include "memmy_ast.h"

typedef struct Memmy_EvalBinding Memmy_EvalBinding;

typedef U32 Memmy_EvalValueKind;
enum
{
    Memmy_EvalValueKind_Null,
    Memmy_EvalValueKind_Const,
    Memmy_EvalValueKind_Address,
    Memmy_EvalValueKind_Range,
    Memmy_EvalValueKind_AddressList,
    Memmy_EvalValueKind_TypedValue,
};

typedef struct Memmy_EvalEnv Memmy_EvalEnv;
struct Memmy_EvalEnv
{
    Arena *arena;
    HashMap bindings; // Memmy_EvalBinding
    B32 has_default_process;
    U32 default_pid;
    Memmy_PointerWidth default_pointer_width;
};

typedef struct Memmy_EvalValue Memmy_EvalValue;
struct Memmy_EvalValue
{
    Memmy_EvalValueKind kind;
    I64 constant;
    Memmy_Addr address;
    Memmy_Range range;
    Memmy_Addr *addresses;
    U64 address_count;
    Memmy_Value typed_value;
    Memmy_Value old_typed_value;
    B32 has_process;
    U32 pid;
    Memmy_PointerWidth pointer_width;
};

typedef U32 Memmy_EvalResultKind;
enum
{
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

typedef struct Memmy_EvalResultSink Memmy_EvalResultSink;
struct Memmy_EvalResultSink
{
    void (*push)(Memmy_EvalResultSink *sink, Memmy_EvalResult result);
    void *user_data;
};

Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena);
void Memmy_EvalEnv_SetDefaultProcess(Memmy_EvalEnv *env, U32 pid, Memmy_PointerWidth pointer_width);
void Memmy_EvalEnv_ClearDefaultProcess(Memmy_EvalEnv *env);
Memmy_Status Memmy_EvalStatement(Memmy_EvalEnv *env, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                 Memmy_Error *error);
Memmy_Status Memmy_EvalExpr(Memmy_EvalEnv *env, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error);
Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value);
Memmy_Status Memmy_EvalEnv_Find(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue *out);
Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name);
void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env);

#endif // MEMMY_EVAL_H
