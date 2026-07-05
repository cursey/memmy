#include "memmy_exec.h"

#include "base_checked.h"
#include "base_list.h"
#include "base_sort.h"

static Memmy_Status Memmy_Exec_TargetCheckProcess(Memmy_Process *process, Memmy_TargetExpr *target, Memmy_Error *error)
{
    if (target->process.kind == Memmy_ProcessSelectorKind_None)
    {
        return Memmy_Status_Ok;
    }

    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing selected process for process-qualified target"));
        return Memmy_Status_InvalidArgument;
    }

    if (target->process.kind == Memmy_ProcessSelectorKind_Pid && process->pid != target->process.pid)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("selected process does not match expression pid selector"));
        return Memmy_Status_InvalidArgument;
    }

    return Memmy_Status_Ok;
}

typedef struct Memmy_ExecModuleResolver Memmy_ExecModuleResolver;
struct Memmy_ExecModuleResolver
{
    Memmy_TargetExpr *target;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};

static Memmy_Status Memmy_ExecModuleResolver_Push(void *user_data, Memmy_Module *module)
{
    Memmy_ExecModuleResolver *resolver = (Memmy_ExecModuleResolver *)user_data;
    if (!String8_EqNoCase(module->name, resolver->target->module_name))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        Memmy_Error_Set(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                        String8_Lit("module target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *module;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ResolveModule(Memmy_Process *process, Memmy_TargetExpr *target, Memmy_Module *out,
                                             Memmy_Error *error)
{
    if (target->kind != Memmy_TargetExprKind_Module)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("range"),
                        String8_Lit("whole-process ranges are not implemented"));
        return Memmy_Status_Unsupported;
    }

    Memmy_Status status = Memmy_Exec_TargetCheckProcess(process, target, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(0, 0);
    Memmy_ExecModuleResolver resolver = {
        .target = target,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_ExecModuleResolver_Push,
        .user_data = &resolver,
    };
    status = Memmy_Process_EnumerateModules(scratch.arena, process, sink, error);
    Scratch_End(scratch);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (resolver.match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = resolver.match;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_AddOffset(Memmy_Addr base, I64 offset, Memmy_Addr *out, Memmy_Error *error)
{
    if (!AddI64ToU64Checked(base, offset, out))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("range"), String8_Lit("range arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static B32 Memmy_Exec_IsReadableRegion(Memmy_Region *region)
{
    return region->state == Memmy_RegionState_Committed && (region->access & Memmy_RegionAccess_Read) != 0 &&
           (region->access & Memmy_RegionAccess_Guard) == 0;
}

static I32 Memmy_Exec_ScanRange_Cmp(void *a, void *b, void *ctx)
{
    Unused(ctx);

    Memmy_Range *range_a = (Memmy_Range *)a;
    Memmy_Range *range_b = (Memmy_Range *)b;
    if (range_a->start < range_b->start)
    {
        return -1;
    }
    if (range_a->start > range_b->start)
    {
        return 1;
    }
    if (range_a->end < range_b->end)
    {
        return -1;
    }
    if (range_a->end > range_b->end)
    {
        return 1;
    }
    return 0;
}

typedef struct Memmy_ExecScanRangeNode Memmy_ExecScanRangeNode;
struct Memmy_ExecScanRangeNode
{
    ListLink link;
    Memmy_Range range;
};

typedef struct Memmy_ExecRegionCollector Memmy_ExecRegionCollector;
struct Memmy_ExecRegionCollector
{
    Arena *arena;
    List ranges; // Memmy_ExecScanRangeNode
    Memmy_Error *error;
};

static Memmy_Status Memmy_ExecRegionCollector_Push(void *user_data, Memmy_Region *region)
{
    Memmy_ExecRegionCollector *collector = (Memmy_ExecRegionCollector *)user_data;
    if (!Memmy_Exec_IsReadableRegion(region))
    {
        return Memmy_Status_Ok;
    }

    Memmy_Range range = {0};
    Memmy_Status status = Memmy_Range_FromStartLength(region->base, region->size, &range, collector->error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ExecScanRangeNode *node = Arena_PushStruct(collector->arena, Memmy_ExecScanRangeNode);
    node->range = range;
    List_PushBack(&collector->ranges, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_WholeProcessRange_Check(Memmy_Process *process, Memmy_RangeExpr *expr,
                                                       Memmy_Error *error)
{
    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                        String8_Lit("missing selected process for whole-process range"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_Exec_TargetCheckProcess(process, &expr->target, error);
}

typedef Memmy_Status Memmy_ExecScanFn(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options, void *needle,
                                      Memmy_ScanSink sink, Memmy_Error *error);

static Memmy_Status Memmy_Exec_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                           void *needle, Memmy_ScanSink sink, Memmy_Error *error)
{
    return Memmy_Process_ScanPattern(arena, process, options, *(Memmy_Pattern *)needle, sink, error);
}

static Memmy_Status Memmy_Exec_ScanValue(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options, void *needle,
                                         Memmy_ScanSink sink, Memmy_Error *error)
{
    return Memmy_Process_ScanValue(arena, process, options, *(Memmy_Value *)needle, sink, error);
}

static Memmy_Status Memmy_Exec_ScanRange(Arena *arena, Memmy_Process *process, Memmy_Range range,
                                         Memmy_ExecScanFn *scan, void *needle, Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanOptions options = {
        .range = range,
        .limit = 0,
        .chunk_size = 0,
    };
    return scan(arena, process, &options, needle, sink, error);
}

static Memmy_Status Memmy_Exec_ScanWholeProcess(Arena *arena, Memmy_Process *process, Memmy_RangeExpr *range_expr,
                                                Memmy_ExecScanFn *scan, void *needle, Memmy_ScanSink sink,
                                                Memmy_Error *error)
{
    Memmy_Status status = Memmy_Exec_WholeProcessRange_Check(process, range_expr, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    Memmy_ExecRegionCollector collector = {
        .arena = scratch.arena,
        .error = error,
    };
    Memmy_RegionSink region_sink = {
        .callback = Memmy_ExecRegionCollector_Push,
        .user_data = &collector,
    };
    status = Memmy_Process_EnumerateRegions(scratch.arena, process, region_sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    Memmy_Range *scan_ranges = Arena_PushArrayNoZero(scratch.arena, Memmy_Range, collector.ranges.count);
    U64 scan_range_count = 0;
    List_ForEach(Memmy_ExecScanRangeNode, node, &collector.ranges, link)
    {
        scan_ranges[scan_range_count++] = node->range;
    }

    if (scan_range_count == 0)
    {
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("scan"), String8_Lit("scan range is unreadable"));
        return Memmy_Status_Unreadable;
    }

    Sort(scan_ranges, scan_range_count, sizeof(scan_ranges[0]), Memmy_Exec_ScanRange_Cmp, 0);
    for (U64 i = 0; i < scan_range_count;)
    {
        Memmy_Range merged_range = scan_ranges[i++];
        while (i < scan_range_count && scan_ranges[i].start <= merged_range.end)
        {
            merged_range.end = Max(merged_range.end, scan_ranges[i].end);
            i++;
        }

        status = Memmy_Exec_ScanRange(arena, process, merged_range, scan, needle, sink, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
    }

    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_RangeExpr_Resolve(Memmy_Process *process, Memmy_RangeExpr *expr, Memmy_Range *out,
                                     Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                        String8_Lit("missing range expression or output range"));
        return Memmy_Status_InvalidArgument;
    }

    if (expr->kind == Memmy_RangeExprKind_Target)
    {
        Memmy_Module module = {0};
        Memmy_Status status = Memmy_Exec_ResolveModule(process, &expr->target, &module, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartLength(module.base, module.size, out, error);
    }

    if (expr->kind == Memmy_RangeExprKind_ModuleOffset || expr->kind == Memmy_RangeExprKind_ModuleSized)
    {
        Memmy_Module module = {0};
        Memmy_Status status = Memmy_Exec_ResolveModule(process, &expr->target, &module, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Addr start = 0;
        status = Memmy_Exec_AddOffset(module.base, expr->start_offset, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        if (expr->kind == Memmy_RangeExprKind_ModuleSized)
        {
            return Memmy_Range_FromStartLength(start, expr->size, out, error);
        }

        Memmy_Addr end = 0;
        status = Memmy_Exec_AddOffset(module.base, expr->end_offset, &end, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartEnd(start, end, out, error);
    }

    if (expr->kind == Memmy_RangeExprKind_AddressSized)
    {
        Memmy_Addr start = 0;
        Memmy_Status status = Memmy_AddressExpr_Resolve(process, &expr->address, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartLength(start, expr->size, out, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                    String8_Lit("unknown range expression kind"));
    return Memmy_Status_InvalidArgument;
}

Memmy_Status Memmy_MemoryExpr_ExecutePatternScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                                 Memmy_ScanSink sink, Memmy_Error *error)
{
    if (expr == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing memory expression or scan sink"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_PatternScan)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("memory expression is not a pattern scan"));
        return Memmy_Status_InvalidArgument;
    }

    if (expr->range.kind == Memmy_RangeExprKind_Target && expr->range.target.kind == Memmy_TargetExprKind_WholeProcess)
    {
        return Memmy_Exec_ScanWholeProcess(arena, process, &expr->range, Memmy_Exec_ScanPattern, &expr->pattern, sink,
                                           error);
    }

    Memmy_Range range = {0};
    Memmy_Status status = Memmy_RangeExpr_Resolve(process, &expr->range, &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    return Memmy_Exec_ScanRange(arena, process, range, Memmy_Exec_ScanPattern, &expr->pattern, sink, error);
}

Memmy_Status Memmy_MemoryExpr_ExecuteValueScan(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                               Memmy_ScanSink sink, Memmy_Error *error)
{
    if (expr == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing memory expression or scan sink"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_ValueScan)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("memory expression is not a value scan"));
        return Memmy_Status_InvalidArgument;
    }
    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                        String8_Lit("missing selected process for value scan"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Value value = {0};
    Memmy_Status status = Memmy_Value_Parse(arena, expr->type, process->pointer_width, expr->value_text, &value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (expr->range.kind == Memmy_RangeExprKind_Target && expr->range.target.kind == Memmy_TargetExprKind_WholeProcess)
    {
        return Memmy_Exec_ScanWholeProcess(arena, process, &expr->range, Memmy_Exec_ScanValue, &value, sink, error);
    }

    Memmy_Range range = {0};
    status = Memmy_RangeExpr_Resolve(process, &expr->range, &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    return Memmy_Exec_ScanRange(arena, process, range, Memmy_Exec_ScanValue, &value, sink, error);
}
