#include "memmy_exec.h"

static void Memmy_ExecRequirements_AddRead(Memmy_ExecRequirements *requirements)
{
    requirements->backend_caps |= Memmy_BackendCap_Read;
    requirements->process_access |= Memmy_ProcessAccess_Read;
}

static void Memmy_ExecRequirements_AddWrite(Memmy_ExecRequirements *requirements)
{
    requirements->backend_caps |= Memmy_BackendCap_Write;
    requirements->process_access |= Memmy_ProcessAccess_Write;
}

static void Memmy_ExecRequirements_AddModules(Memmy_ExecRequirements *requirements)
{
    requirements->backend_caps |= Memmy_BackendCap_ListModules;
    requirements->process_access |= Memmy_ProcessAccess_Query;
    requirements->needs_modules = 1;
}

static void Memmy_ExecRequirements_AddRegions(Memmy_ExecRequirements *requirements)
{
    requirements->backend_caps |= Memmy_BackendCap_ListRegions;
    requirements->process_access |= Memmy_ProcessAccess_Query;
    requirements->needs_regions = 1;
}

static void Memmy_TargetExpr_AddRequirements(Memmy_TargetExpr *target, Memmy_ExecRequirements *requirements)
{
    if (target->kind == Memmy_TargetExprKind_Module)
    {
        Memmy_ExecRequirements_AddModules(requirements);
        if (target->process.kind == Memmy_ProcessSelectorKind_None)
        {
            requirements->needs_external_process = 1;
        }
    }
}

static void Memmy_AddressExpr_AddRequirements(Memmy_AddressExpr *address, Memmy_ExecRequirements *requirements)
{
    if (address->base_kind == Memmy_AddressExprBaseKind_Absolute)
    {
        requirements->needs_external_process = 1;
    }
    else if (address->base_kind == Memmy_AddressExprBaseKind_Target)
    {
        Memmy_TargetExpr_AddRequirements(&address->target, requirements);
    }

    for (Memmy_AddressOp *op = (Memmy_AddressOp *)address->ops.first; op != 0; op = (Memmy_AddressOp *)op->link.next)
    {
        if (op->kind == Memmy_AddressOpKind_Deref || op->kind == Memmy_AddressOpKind_DerefOffset)
        {
            Memmy_ExecRequirements_AddRead(requirements);
        }
    }
}

static void Memmy_RangeExpr_AddRequirements(Memmy_RangeExpr *range, Memmy_ExecRequirements *requirements)
{
    if (range->kind == Memmy_RangeExprKind_Target)
    {
        if (range->target.kind == Memmy_TargetExprKind_WholeProcess)
        {
            Memmy_ExecRequirements_AddRegions(requirements);
        }
        else
        {
            Memmy_TargetExpr_AddRequirements(&range->target, requirements);
        }
    }
    else if (range->kind == Memmy_RangeExprKind_ModuleOffset || range->kind == Memmy_RangeExprKind_ModuleSized)
    {
        Memmy_TargetExpr_AddRequirements(&range->target, requirements);
    }
    else if (range->kind == Memmy_RangeExprKind_AddressSized)
    {
        Memmy_AddressExpr_AddRequirements(&range->address, requirements);
    }
}

Memmy_Status Memmy_MemoryExpr_GetRequirements(Memmy_MemoryExpr *expr, Memmy_ExecRequirements *out, Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing expression or output requirements"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExecRequirements requirements = {0};
    if (expr->kind == Memmy_MemoryExprKind_Address)
    {
        Memmy_AddressExpr_AddRequirements(&expr->address, &requirements);
    }
    else if (expr->kind == Memmy_MemoryExprKind_Peek)
    {
        Memmy_AddressExpr_AddRequirements(&expr->address, &requirements);
        Memmy_ExecRequirements_AddRead(&requirements);
    }
    else if (expr->kind == Memmy_MemoryExprKind_Poke)
    {
        Memmy_AddressExpr_AddRequirements(&expr->address, &requirements);
        Memmy_ExecRequirements_AddRead(&requirements);
        Memmy_ExecRequirements_AddWrite(&requirements);
    }
    else if (expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        Memmy_RangeExpr_AddRequirements(&expr->range, &requirements);
        Memmy_ExecRequirements_AddRead(&requirements);
    }

    *out = requirements;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
