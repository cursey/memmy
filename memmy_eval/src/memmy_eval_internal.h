#ifndef MEMMY_EVAL_INTERNAL_H
#define MEMMY_EVAL_INTERNAL_H

#include "memmy_eval.h"

#include "base_hash.h"
#include "base_hashmap.h"
#include "base_list.h"

#define MEMMY_EVAL_STRING_READ_MAX 4096
#define MEMMY_EVAL_STRING_READ_CHUNK_SIZE 256

typedef struct Memmy_EvalBinding Memmy_EvalBinding;
struct Memmy_EvalBinding
{
    HashLink hash;
    String8 name;
    Memmy_EvalValue value;
};
struct Memmy_EvalEnv
{
    Arena *arena;
    HashMap bindings; // Memmy_EvalBinding
    B32 has_default_process;
    U32 default_pid;
    Memmy_PointerWidth default_pointer_width;
};
typedef struct Memmy_EvalModuleResolver Memmy_EvalModuleResolver;
struct Memmy_EvalModuleResolver
{
    String8 name;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};
typedef struct Memmy_EvalScanResultNode Memmy_EvalScanResultNode;
struct Memmy_EvalScanResultNode
{
    ListLink link;
    Memmy_Addr address;
};
typedef struct Memmy_EvalAddressNode Memmy_EvalAddressNode;
struct Memmy_EvalAddressNode
{
    ListLink link;
    Memmy_Addr address;
};
typedef struct Memmy_EvalRangeNode Memmy_EvalRangeNode;
struct Memmy_EvalRangeNode
{
    ListLink link;
    Memmy_Range range;
};
typedef struct Memmy_EvalScanCollector Memmy_EvalScanCollector;
struct Memmy_EvalScanCollector
{
    Arena *arena;
    List addresses; // Memmy_EvalScanResultNode
};
typedef struct Memmy_EvalByteReader Memmy_EvalByteReader;
struct Memmy_EvalByteReader
{
    Memmy_Process *process;
    Memmy_Addr address;
    U64 offset;
    U8 buffer[MEMMY_EVAL_STRING_READ_CHUNK_SIZE];
    U64 pos;
    U64 count;
    Memmy_Status terminal_status;
};
typedef struct Memmy_EvalProcessEmitter Memmy_EvalProcessEmitter;
struct Memmy_EvalProcessEmitter
{
    Memmy_EvalResultSink const *sink;
    String8 filter;
};
typedef struct Memmy_EvalModuleEmitter Memmy_EvalModuleEmitter;
struct Memmy_EvalModuleEmitter
{
    Memmy_EvalResultSink const *sink;
    String8 filter;
};
typedef struct Memmy_EvalRegionEmitter Memmy_EvalRegionEmitter;
struct Memmy_EvalRegionEmitter
{
    Memmy_EvalResultSink const *sink;
};
typedef struct Memmy_EvalOpenProcess Memmy_EvalOpenProcess;
struct Memmy_EvalOpenProcess
{
    ListLink link;
    Memmy_Process *process;
};
typedef struct Memmy_EvalExec Memmy_EvalExec;
struct Memmy_EvalExec
{
    Memmy_EvalEnv *env;
    Arena *out_arena;
    Arena *transient_arena;
    List open_processes; // Memmy_EvalOpenProcess
    B32 has_current_item;
    Memmy_EvalValue current_item;
};

void Memmy_EvalError(Memmy_Error *error, Memmy_Status status, String8 context, String8 message);
void Memmy_EvalExec_Close(Memmy_EvalExec *exec);
Memmy_Status Memmy_EvalExec_OpenProcess(Memmy_EvalExec *exec, U32 pid, Memmy_Process **out, Memmy_Error *error);
Memmy_Status Memmy_Eval_RequireProcess(Memmy_EvalExec *exec, Memmy_EvalValue *value, String8 context,
                                       Memmy_Process **out, Memmy_Error *error);
Memmy_Status Memmy_Eval_EmitValueResult(Memmy_EvalResultSink const *sink, Memmy_EvalValue value);
Memmy_Status Memmy_Eval_Command(Memmy_EvalExec *exec, Memmy_AstStatement const *statement,
                                Memmy_EvalResultSink const *sink, Memmy_Error *error);
Memmy_Status Memmy_EvalExprWithContext(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                       Memmy_Error *error);
Memmy_Status Memmy_EvalStatementWithContext(Memmy_EvalExec *exec, Memmy_AstStatement const *statement,
                                            Memmy_EvalResultSink const *sink, Memmy_Error *error);
Memmy_Status Memmy_EvalValue_AsConst(Memmy_EvalValue *value, I64 *out, Memmy_Error *error);
B32 Memmy_EvalValue_IsIntegerTyped(Memmy_EvalValue *value);
Memmy_Status Memmy_EvalValue_AsAddress(Memmy_EvalValue *value, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_Eval_AddressAddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_Eval_ApplyBinary(Memmy_AstConstOp op, Memmy_EvalValue lhs, Memmy_EvalValue rhs, Memmy_EvalValue *out,
                                    Memmy_Error *error);
Memmy_Status Memmy_Eval_ListTransform(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                      Memmy_Error *error);
Memmy_Status Memmy_Eval_Target(Memmy_EvalExec *exec, Memmy_AstNode const *target, Memmy_EvalValue *out,
                               Memmy_Error *error);
Memmy_Status Memmy_Eval_ParseType(String8 type_name, Memmy_Type *out, Memmy_Error *error);
I64 Memmy_Eval_IntegerFromBytes(Memmy_Value value);
Memmy_Status Memmy_Eval_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                  Memmy_Value *out, Memmy_Error *error);
Memmy_Status Memmy_Eval_ParseValue(Memmy_EvalExec *exec, Memmy_Process *process, Memmy_Type type, String8 text,
                                   Memmy_Value *out, Memmy_Error *error);
Memmy_Status Memmy_Eval_ReadPointer(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_Eval_ValueExpr(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                  Memmy_Error *error);
Memmy_Status Memmy_Eval_MemoryExpr(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                   Memmy_Error *error);
Memmy_Status Memmy_Eval_ProcessExpr(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                    Memmy_Error *error);
Memmy_Status Memmy_Eval_ScanExpr(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                 Memmy_Error *error);
Memmy_Status Memmy_Eval_DisasmX64Scan(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                      Memmy_AstDisasmPattern pattern, Memmy_ScanSink sink, Memmy_Error *error);

#endif
