#include "memmy_exec.h"

#include "base_checked.h"

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

static Memmy_Status Memmy_Exec_ResolveModule(Memmy_Process *process, Memmy_ModuleList *modules,
                                             Memmy_TargetExpr *target, Memmy_Module **out, Memmy_Error *error)
{
    if (target->kind != Memmy_TargetExprKind_Module)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("range"),
                        String8_Lit("whole-process ranges are not implemented"));
        return Memmy_Status_Unsupported;
    }
    if (modules == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing module list for module target"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Status status = Memmy_Exec_TargetCheckProcess(process, target, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Module *match = 0;
    List_ForEach(Memmy_Module, module, &modules->list, link)
    {
        if (String8_EqNoCase(module->name, target->module_name))
        {
            if (match != 0)
            {
                Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("target"),
                                String8_Lit("module target is ambiguous"));
                return Memmy_Status_Ambiguous;
            }
            match = module;
        }
    }

    if (match == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = match;
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

Memmy_Status Memmy_RangeExpr_Resolve(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_RangeExpr *expr,
                                     Memmy_Range *out, Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                        String8_Lit("missing range expression or output range"));
        return Memmy_Status_InvalidArgument;
    }

    if (expr->kind == Memmy_RangeExprKind_Target)
    {
        Memmy_Module *module = 0;
        Memmy_Status status = Memmy_Exec_ResolveModule(process, modules, &expr->target, &module, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartLength(module->base, module->size, out, error);
    }

    if (expr->kind == Memmy_RangeExprKind_ModuleOffset || expr->kind == Memmy_RangeExprKind_ModuleSized)
    {
        Memmy_Module *module = 0;
        Memmy_Status status = Memmy_Exec_ResolveModule(process, modules, &expr->target, &module, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Addr start = 0;
        status = Memmy_Exec_AddOffset(module->base, expr->start_offset, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        if (expr->kind == Memmy_RangeExprKind_ModuleSized)
        {
            return Memmy_Range_FromStartLength(start, expr->size, out, error);
        }

        Memmy_Addr end = 0;
        status = Memmy_Exec_AddOffset(module->base, expr->end_offset, &end, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartEnd(start, end, out, error);
    }

    if (expr->kind == Memmy_RangeExprKind_AddressSized)
    {
        Memmy_Addr start = 0;
        Memmy_Status status = Memmy_AddressExpr_Resolve(process, modules, &expr->address, &start, error);
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

Memmy_Status Memmy_MemoryExpr_ExecutePatternScan(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                                 Memmy_MemoryExpr *expr, Memmy_ScanResultList *out, Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing memory expression or scan result output"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_PatternScan)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("memory expression is not a pattern scan"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Range range = {0};
    Memmy_Status status = Memmy_RangeExpr_Resolve(process, modules, &expr->range, &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ScanOptions options = {
        .range = range,
        .limit = 0,
        .chunk_size = 0,
    };
    return Memmy_Process_ScanPattern(arena, process, &options, expr->pattern, out, error);
}
