#include "memmy_eval_internal.h"

#include "base.h"

static Memmy_Status MemmyEval_ScanCollector_Push(void *user_data, Memmy_Addr address)
{
    MemmyEval_ScanCollector *collector = (MemmyEval_ScanCollector *)user_data;
    MemmyEval_ScanResultNode *node = Arena_PushStruct(collector->arena, MemmyEval_ScanResultNode);
    node->address = address;
    List_PushBack(&collector->addresses, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_ReferenceScanMode MemmyEval_ReferenceScanMode(MemmyAst_ReferenceMode mode)
{
    switch (mode)
    {
    case MemmyAst_ReferenceMode_Ptr:
        return Memmy_ReferenceScanMode_Ptr;
    case MemmyAst_ReferenceMode_Rel32:
        return Memmy_ReferenceScanMode_Rel32;
    case MemmyAst_ReferenceMode_Any:
        return Memmy_ReferenceScanMode_Any;
    default:
        return Memmy_ReferenceScanMode_Any;
    }
}

static MemmyEval_Value MemmyEval_AddressList_FromCollector(Arena *arena, MemmyEval_ScanCollector *collector)
{
    Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, collector->addresses.count);
    U64 index = 0;
    List_ForEach(MemmyEval_ScanResultNode, node, &collector->addresses, link)
    {
        addresses[index++] = node->address;
    }

    return (MemmyEval_Value){
        .kind = MemmyEval_ValueKind_AddressList,
        .addresses = addresses,
        .address_count = collector->addresses.count,
    };
}

Memmy_Status MemmyEval_Expr_EvalScan(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                     Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    (void)env;
    if (expr->kind == MemmyAst_NodeKind_PatternScan)
    {
        MemmyEval_Value range_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != MemmyEval_ValueKind_Range)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                                String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Pattern pattern = {0};
        status = Memmy_Pattern_Parse(exec->transient_arena, expr->pattern, Memmy_PatternParseFlag_AllowWildcards,
                                     &pattern, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_ScanCollector collector = {.arena = exec->transient_arena};
        Memmy_ScanSink sink = {
            .callback = MemmyEval_ScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
        };
        status = Memmy_Process_ScanPattern(exec->transient_arena, process, &options, pattern, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = MemmyEval_AddressList_FromCollector(exec->out_arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ValueScan)
    {
        MemmyEval_Value range_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != MemmyEval_ValueKind_Range)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                                String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Type type = {0};
        status = MemmyEval_Type_Parse(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EncodedValue value = {0};
        status = MemmyEval_Value_Parse(exec, process, type, expr->value_text, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_ScanCollector collector = {.arena = exec->transient_arena};
        Memmy_ScanSink sink = {
            .callback = MemmyEval_ScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
        };
        status = Memmy_Process_ScanValue(exec->transient_arena, process, &options, value, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = MemmyEval_AddressList_FromCollector(exec->out_arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ReferenceScan)
    {
        MemmyEval_Value range_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != MemmyEval_ValueKind_Range)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                                String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        MemmyEval_Value target_value = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &target_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr target = 0;
        status = MemmyEval_Value_AsAddress(&target_value, &target, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_ScanCollector collector = {.arena = exec->transient_arena};
        Memmy_ScanSink sink = {
            .callback = MemmyEval_ScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
        };
        status = Memmy_Process_ScanReferences(exec->transient_arena, process, &options,
                                              MemmyEval_ReferenceScanMode(expr->reference_mode), target, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = MemmyEval_AddressList_FromCollector(exec->out_arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_DisasmScan)
    {
        MemmyEval_Value range_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, &range_value, String8_Lit("disasm"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != MemmyEval_ValueKind_Range)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("disasm"),
                                String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        MemmyEval_ScanCollector collector = {.arena = exec->transient_arena};
        Memmy_ScanSink sink = {
            .callback = MemmyEval_ScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
        };
        status = MemmyEval_DisasmX64_Scan(exec->transient_arena, process, &options, expr->disasm_pattern, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = MemmyEval_AddressList_FromCollector(exec->out_arena, &collector);
        return Memmy_Status_Ok;
    }
    MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
