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
    default:
        return Memmy_ReferenceScanMode_Any;
    }
}

static Memmy_Status MemmyEval_AddressList_FromCollector(Arena *arena, MemmyEval_ScanCollector *collector,
                                                        Memmy_Value *out, Memmy_Error *error)
{
    Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, collector->addresses.count);
    U64 index = 0;
    List_ForEach(MemmyEval_ScanResultNode, node, &collector->addresses, link)
    {
        addresses[index++] = node->address;
    }
    Memmy_Type type = {0};
    Memmy_Status status = Memmy_Type_ListCreate(arena, Memmy_Type_Address, &type, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = (Memmy_Value){.type = type, .list = {.count = collector->addresses.count, .addresses = addresses}};
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_ScanRange(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Range *out,
                                        Memmy_Process **out_process, Memmy_Error *error)
{
    Memmy_Value range = {0};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!Memmy_Type_IsRange(range.type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
        return Memmy_Status_InvalidArgument;
    }
    status = MemmyEval_Process_Require(exec, String8_Lit("scan"), out_process, error);
    if (status == Memmy_Status_Ok)
    {
        *out = range.range;
    }
    return status;
}

Memmy_Status MemmyEval_Expr_EvalScan(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out,
                                     Memmy_Error *error)
{
    Memmy_Range range = {0};
    Memmy_Process *process = 0;
    Memmy_Status status = MemmyEval_ScanRange(exec, expr, &range, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    MemmyEval_ScanCollector collector = {.arena = exec->transient_arena};
    Memmy_ScanSink sink = {.callback = MemmyEval_ScanCollector_Push, .user_data = &collector};
    Memmy_ScanOptions options = {.range = range};
    if (expr->kind == MemmyAst_NodeKind_PatternScan)
    {
        Memmy_Pattern pattern = {0};
        status = Memmy_Pattern_Parse(exec->transient_arena, expr->pattern, Memmy_PatternParseFlag_AllowWildcards,
                                     &pattern, error);
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_Process_ScanPattern(exec->transient_arena, process, &options, pattern, sink, error);
        }
    }
    else if (expr->kind == MemmyAst_NodeKind_ValueScan)
    {
        Memmy_Type type = {0};
        status = Memmy_Type_Parse(expr->type_name, &type, error);
        Memmy_Value value = {0};
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &value, error);
        }
        Memmy_Value converted = {0};
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_Value_Convert(exec->transient_arena, &value, type, &converted, error);
        }
        Memmy_EncodedValue needle = {0};
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_Value_Encode(exec->transient_arena, &converted, &needle, error);
        }
        if (status == Memmy_Status_Ok && Memmy_Type_IsString(type) && type.string.zero_terminated)
        {
            needle.bytes.len -= type.string.encoding == Memmy_StringEncoding_Utf8 ? 1 : 2;
        }
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_Process_ScanValue(exec->transient_arena, process, &options, needle, sink, error);
        }
    }
    else if (expr->kind == MemmyAst_NodeKind_ReferenceScan)
    {
        Memmy_Value target_value = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &target_value, error);
        Memmy_Addr target = 0;
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Address_FromValue(&target_value, &target, error);
        }
        if (status == Memmy_Status_Ok)
        {
            status =
                Memmy_Process_ScanReferences(exec->transient_arena, process, &options,
                                             MemmyEval_ReferenceScanMode(expr->reference_mode), target, sink, error);
        }
    }
    else if (expr->kind == MemmyAst_NodeKind_DisasmScan)
    {
        status = MemmyEval_DisasmX64_Scan(exec->transient_arena, process, &options, expr->disasm_pattern, sink, error);
    }
    else
    {
        MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                            String8_Lit("expression kind is not implemented yet"));
        return Memmy_Status_Unsupported;
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return MemmyEval_AddressList_FromCollector(exec->out_arena, &collector, out, error);
}
