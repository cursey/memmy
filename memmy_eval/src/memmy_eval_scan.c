#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

static Memmy_Status Memmy_EvalScanCollector_Push(void *user_data, Memmy_Addr address)
{
    Memmy_EvalScanCollector *collector = (Memmy_EvalScanCollector *)user_data;
    Memmy_EvalScanResultNode *node = Arena_PushStruct(collector->arena, Memmy_EvalScanResultNode);
    node->address = address;
    List_PushBack(&collector->addresses, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_ReferenceScanMode Memmy_Eval_ReferenceScanMode(Memmy_AstReferenceMode mode)
{
    switch (mode)
    {
    case Memmy_AstReferenceMode_Ptr:
        return Memmy_ReferenceScanMode_Ptr;
    case Memmy_AstReferenceMode_Rel32:
        return Memmy_ReferenceScanMode_Rel32;
    case Memmy_AstReferenceMode_Any:
        return Memmy_ReferenceScanMode_Any;
    default:
        return Memmy_ReferenceScanMode_Any;
    }
}

static Memmy_EvalValue Memmy_Eval_AddressListFromCollector(Arena *arena, Memmy_EvalScanCollector *collector)
{
    Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, collector->addresses.count);
    U64 index = 0;
    List_ForEach(Memmy_EvalScanResultNode, node, &collector->addresses, link)
    {
        addresses[index++] = node->address;
    }

    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_AddressList,
        .addresses = addresses,
        .address_count = collector->addresses.count,
    };
}

Memmy_Status Memmy_Eval_ScanExpr(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)env;
    if (expr->kind == Memmy_AstNodeKind_PatternScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Pattern pattern = {0};
        status = Memmy_Pattern_Parse(env->arena, expr->pattern, Memmy_PatternParseFlag_AllowWildcards, &pattern, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanPattern(env->arena, process, &options, pattern, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ValueScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value value = {0};
        status = Memmy_Eval_ParseValue(exec, process, type, expr->value_text, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanValue(env->arena, process, &options, value, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ReferenceScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_EvalValue target_value = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &target_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr target = 0;
        status = Memmy_EvalValue_AsAddress(&target_value, &target, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanReferences(env->arena, process, &options,
                                              Memmy_Eval_ReferenceScanMode(expr->reference_mode), target, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_DisasmScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("disasm"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("disasm"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Eval_DisasmX64Scan(env->arena, process, &options, expr->disasm_pattern, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
