#ifndef MEMMY_EVAL_INTERNAL_H
#define MEMMY_EVAL_INTERNAL_H

#include "memmy_eval.h"

#include "base.h"

#define MEMMY_EVAL_STRING_READ_MAX 4096
#define MEMMY_EVAL_STRING_READ_CHUNK_SIZE 256

typedef struct MemmyEval_Binding MemmyEval_Binding;
struct MemmyEval_Binding
{
    HashLink hash;
    String8 name;
    MemmyEval_Value value;
};
struct MemmyEval_Env
{
    Arena *arena;
    HashMap bindings; // MemmyEval_Binding
    B32 has_default_process;
    U32 default_pid;
    Memmy_PointerWidth default_pointer_width;
};
typedef struct MemmyEval_ModuleResolver MemmyEval_ModuleResolver;
struct MemmyEval_ModuleResolver
{
    String8 name;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};
typedef struct MemmyEval_ScanResultNode MemmyEval_ScanResultNode;
struct MemmyEval_ScanResultNode
{
    ListLink link;
    Memmy_Addr address;
};
typedef struct MemmyEval_AddressNode MemmyEval_AddressNode;
struct MemmyEval_AddressNode
{
    ListLink link;
    Memmy_Addr address;
};
typedef struct MemmyEval_RangeNode MemmyEval_RangeNode;
struct MemmyEval_RangeNode
{
    ListLink link;
    Memmy_Range range;
};
typedef struct MemmyEval_ScanCollector MemmyEval_ScanCollector;
struct MemmyEval_ScanCollector
{
    Arena *arena;
    List addresses; // MemmyEval_ScanResultNode
};
typedef struct MemmyEval_ByteReader MemmyEval_ByteReader;
struct MemmyEval_ByteReader
{
    Memmy_Process *process;
    Memmy_Addr address;
    U64 offset;
    U8 buffer[MEMMY_EVAL_STRING_READ_CHUNK_SIZE];
    U64 pos;
    U64 count;
    Memmy_Status terminal_status;
};
typedef struct MemmyEval_ProcessEmitter MemmyEval_ProcessEmitter;
struct MemmyEval_ProcessEmitter
{
    MemmyEval_ResultSink const *sink;
    String8 filter;
};
typedef struct MemmyEval_ModuleEmitter MemmyEval_ModuleEmitter;
struct MemmyEval_ModuleEmitter
{
    MemmyEval_ResultSink const *sink;
    String8 filter;
};
typedef struct MemmyEval_RegionEmitter MemmyEval_RegionEmitter;
struct MemmyEval_RegionEmitter
{
    MemmyEval_ResultSink const *sink;
};
typedef struct MemmyEval_OpenProcess MemmyEval_OpenProcess;
struct MemmyEval_OpenProcess
{
    ListLink link;
    Memmy_Process *process;
};
typedef struct MemmyEval_Exec MemmyEval_Exec;
struct MemmyEval_Exec
{
    MemmyEval_Env *env;
    Arena *out_arena;
    Arena *transient_arena;
    List open_processes; // MemmyEval_OpenProcess
    B32 has_current_item;
    MemmyEval_Value current_item;
};

void MemmyEval_Error_Set(Memmy_Error *error, Memmy_Status status, String8 context, String8 message);
void MemmyEval_Exec_Close(MemmyEval_Exec *exec);
Memmy_Status MemmyEval_Exec_OpenProcess(MemmyEval_Exec *exec, U32 pid, Memmy_Process **out, Memmy_Error *error);
Memmy_Status MemmyEval_Process_Require(MemmyEval_Exec *exec, MemmyEval_Value *value, String8 context,
                                       Memmy_Process **out, Memmy_Error *error);
Memmy_Status MemmyEval_ValueResult_Emit(MemmyEval_ResultSink const *sink, MemmyEval_Value value);
Memmy_Status MemmyEval_Command_Eval(MemmyEval_Exec *exec, MemmyAst_Statement const *statement,
                                    MemmyEval_ResultSink const *sink, Memmy_Error *error);
Memmy_Status MemmyEval_Expr_EvalWithContext(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                            Memmy_Error *error);
Memmy_Status MemmyEval_Statement_EvalWithContext(MemmyEval_Exec *exec, MemmyAst_Statement const *statement,
                                                 MemmyEval_ResultSink const *sink, Memmy_Error *error);
Memmy_Status MemmyEval_Value_AsConst(MemmyEval_Value *value, I64 *out, Memmy_Error *error);
B32 MemmyEval_Value_IsIntegerTyped(MemmyEval_Value *value);
Memmy_Status MemmyEval_Value_AsAddress(MemmyEval_Value *value, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status MemmyEval_Address_AddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status MemmyEval_Value_ApplyBinary(MemmyAst_ConstOp op, MemmyEval_Value lhs, MemmyEval_Value rhs,
                                         MemmyEval_Value *out, Memmy_Error *error);
Memmy_Status MemmyEval_List_Transform(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                      Memmy_Error *error);
Memmy_Status MemmyEval_Target_Eval(MemmyEval_Exec *exec, MemmyAst_Node const *target, MemmyEval_Value *out,
                                   Memmy_Error *error);
Memmy_Status MemmyEval_Type_Parse(String8 type_name, Memmy_Type *out, Memmy_Error *error);
I64 MemmyEval_Integer_FromBytes(Memmy_Value value);
Memmy_Status MemmyEval_Value_Read(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                  Memmy_Value *out, Memmy_Error *error);
Memmy_Status MemmyEval_Value_Parse(MemmyEval_Exec *exec, Memmy_Process *process, Memmy_Type type, String8 text,
                                   Memmy_Value *out, Memmy_Error *error);
Memmy_Status MemmyEval_Pointer_Read(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status MemmyEval_Expr_EvalValue(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                      Memmy_Error *error);
Memmy_Status MemmyEval_Expr_EvalMemory(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                       Memmy_Error *error);
Memmy_Status MemmyEval_Expr_EvalProcess(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                        Memmy_Error *error);
Memmy_Status MemmyEval_Expr_EvalScan(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                     Memmy_Error *error);
Memmy_Status MemmyEval_DisasmX64_Scan(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                      MemmyAst_DisasmPattern pattern, Memmy_ScanSink sink, Memmy_Error *error);

#endif
