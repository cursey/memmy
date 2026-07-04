#include "memmy_cli_internal.h"

#include "memmy_exec.h"

Memmy_Status Memmy_Cli_RejectNonExprOptions(Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options->has_limit)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for expression input"),
                                       String8_Lit("--limit"));
    }
    if (options->has_chunk_size)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for expression input"),
                                       String8_Lit("--chunk-size"));
    }
    if (options->has_filter || options->has_addr || options->has_type || options->has_count || options->has_value ||
        options->dry_run || options->has_start || options->has_end || options->has_length || options->has_pattern)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for --expr"), (String8){0});
    }
    return Memmy_Status_Ok;
}

static Memmy_ProcessSelector Memmy_Cli_ExprProcessSelector(Memmy_MemoryExpr *expr)
{
    Memmy_ProcessSelector selector = {0};
    if ((expr->kind == Memmy_MemoryExprKind_Address || expr->kind == Memmy_MemoryExprKind_Peek ||
         expr->kind == Memmy_MemoryExprKind_Poke) &&
        expr->address.base_kind == Memmy_AddressExprBaseKind_Target)
    {
        selector = expr->address.target.process;
    }
    else if ((expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan))
    {
        if (expr->range.kind == Memmy_RangeExprKind_Target || expr->range.kind == Memmy_RangeExprKind_ModuleOffset ||
            expr->range.kind == Memmy_RangeExprKind_ModuleSized)
        {
            selector = expr->range.target.process;
        }
        else if (expr->range.kind == Memmy_RangeExprKind_AddressSized &&
                 expr->range.address.base_kind == Memmy_AddressExprBaseKind_Target)
        {
            selector = expr->range.address.target.process;
        }
    }
    return selector;
}

typedef struct Memmy_CliExprNeeds Memmy_CliExprNeeds;
struct Memmy_CliExprNeeds
{
    B32 process;
    B32 modules;
    B32 regions;
};

static void Memmy_Cli_TargetExprNeeds(Memmy_TargetExpr *target, Memmy_CliExprNeeds *needs)
{
    if (target->kind == Memmy_TargetExprKind_Module)
    {
        needs->process = 1;
        needs->modules = 1;
    }
    else if (target->kind == Memmy_TargetExprKind_WholeProcess)
    {
        needs->process = 1;
        needs->regions = 1;
    }
}

static void Memmy_Cli_AddressExprNeeds(Memmy_AddressExpr *address, Memmy_CliExprNeeds *needs)
{
    if (address->base_kind == Memmy_AddressExprBaseKind_Target)
    {
        Memmy_Cli_TargetExprNeeds(&address->target, needs);
    }

    List_ForEach(Memmy_AddressOp, op, &address->ops, link)
    {
        if (op->kind == Memmy_AddressOpKind_Deref || op->kind == Memmy_AddressOpKind_DerefOffset)
        {
            needs->process = 1;
        }
    }
}

static void Memmy_Cli_RangeExprNeeds(Memmy_RangeExpr *range, Memmy_CliExprNeeds *needs)
{
    if (range->kind == Memmy_RangeExprKind_Target)
    {
        Memmy_Cli_TargetExprNeeds(&range->target, needs);
    }
    else if (range->kind == Memmy_RangeExprKind_ModuleOffset || range->kind == Memmy_RangeExprKind_ModuleSized)
    {
        Memmy_Cli_TargetExprNeeds(&range->target, needs);
    }
    else if (range->kind == Memmy_RangeExprKind_AddressSized)
    {
        Memmy_Cli_AddressExprNeeds(&range->address, needs);
    }
}

static Memmy_CliExprNeeds Memmy_Cli_MemoryExprNeeds(Memmy_MemoryExpr *expr)
{
    Memmy_CliExprNeeds needs = {0};
    if (expr->kind == Memmy_MemoryExprKind_Address)
    {
        Memmy_Cli_AddressExprNeeds(&expr->address, &needs);
    }
    else if (expr->kind == Memmy_MemoryExprKind_Peek || expr->kind == Memmy_MemoryExprKind_Poke)
    {
        needs.process = 1;
        Memmy_Cli_AddressExprNeeds(&expr->address, &needs);
    }
    else if (expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        needs.process = 1;
        Memmy_Cli_RangeExprNeeds(&expr->range, &needs);
    }
    return needs;
}

static Memmy_Status Memmy_Cli_ResolveProcessName(Arena *arena, String8 name, U32 *out_pid, Memmy_Error *error)
{
    Memmy_ProcessList processes = {0};
    Memmy_Status status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U32 match_pid = 0;
    U64 match_count = 0;
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (String8_EqNoCase(info->name, name))
        {
            match_pid = info->pid;
            match_count++;
        }
    }

    if (match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("process"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (match_count > 1)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("process"),
                        String8_Lit("process name is ambiguous"));
        return Memmy_Status_Ambiguous;
    }

    *out_pid = match_pid;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ResolveProcessSelector(Arena *arena, Memmy_ProcessSelector selector, U32 *out_pid,
                                                     Memmy_Error *error)
{
    if (selector.kind == Memmy_ProcessSelectorKind_Pid)
    {
        *out_pid = selector.pid;
        return Memmy_Status_Ok;
    }
    if (selector.kind == Memmy_ProcessSelectorKind_Name)
    {
        return Memmy_Cli_ResolveProcessName(arena, selector.name, out_pid, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                    String8_Lit("missing process selector"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Cli_SelectExprProcess(Arena *arena, Memmy_CliOptions *options,
                                                Memmy_ProcessSelector expr_selector, B32 process_required, U32 *out_pid,
                                                Memmy_Error *error)
{
    if (options->has_pid && options->has_name)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"),
                        String8_Lit("use --pid or --name, not both"));
        return Memmy_Status_ParseError;
    }

    B32 has_external = options->has_pid || options->has_name;
    U32 external_pid = 0;
    if (options->has_pid)
    {
        external_pid = options->pid;
    }
    else if (options->has_name)
    {
        Memmy_Status status = Memmy_Cli_ResolveProcessName(arena, options->name, &external_pid, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    B32 has_expr_selector = expr_selector.kind != Memmy_ProcessSelectorKind_None;
    U32 expr_pid = 0;
    if (has_expr_selector)
    {
        Memmy_Status status = Memmy_Cli_ResolveProcessSelector(arena, expr_selector, &expr_pid, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    if (has_external && has_expr_selector && external_pid != expr_pid)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"),
                        String8_Lit("external process selector conflicts with expression process selector"));
        return Memmy_Status_InvalidArgument;
    }

    if (has_external)
    {
        *out_pid = external_pid;
        return Memmy_Status_Ok;
    }
    if (has_expr_selector)
    {
        *out_pid = expr_pid;
        return Memmy_Status_Ok;
    }
    if (process_required)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"),
                        String8_Lit("--expr requires --pid or --name"));
        return Memmy_Status_ParseError;
    }

    *out_pid = 0;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RunExpr(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RejectNonExprOptions(options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_MemoryExpr expr = {0};
    status = Memmy_MemoryExpr_Parse(arena, options->expr_text, &expr, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (expr.kind != Memmy_MemoryExprKind_Address && expr.kind != Memmy_MemoryExprKind_Peek &&
        expr.kind != Memmy_MemoryExprKind_Poke && expr.kind != Memmy_MemoryExprKind_PatternScan &&
        expr.kind != Memmy_MemoryExprKind_ValueScan)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("only address, peek, poke, and scan expressions are implemented"));
        return Memmy_Status_Unsupported;
    }
    if (options->jsonl && expr.kind != Memmy_MemoryExprKind_PatternScan && expr.kind != Memmy_MemoryExprKind_ValueScan)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("--jsonl is only valid for scan expressions"),
                                       String8_Lit("--jsonl"));
    }

    Memmy_CliExprNeeds needs = Memmy_Cli_MemoryExprNeeds(&expr);
    Memmy_ProcessSelector expr_selector = Memmy_Cli_ExprProcessSelector(&expr);
    B32 process_required = needs.process || expr_selector.kind != Memmy_ProcessSelectorKind_None;
    U32 pid = 0;
    status = Memmy_Cli_SelectExprProcess(arena, options, expr_selector, process_required, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    Memmy_PointerWidth pointer_width = Memmy_PointerWidth_64;
    if (process_required)
    {
        status = Memmy_Process_Open(arena, pid, &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        pointer_width = process->pointer_width;
    }

    Memmy_ModuleList modules = {0};
    Memmy_ModuleList *modules_ptr = 0;
    if (needs.modules)
    {
        status = Memmy_Process_ListModules(arena, process, &modules, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Process_Close(process);
            return status;
        }
        modules_ptr = &modules;
    }

    Memmy_RegionList regions = {0};
    Memmy_RegionList *regions_ptr = 0;
    if (needs.regions)
    {
        status = Memmy_Process_ListRegions(arena, process, &regions, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Process_Close(process);
            return status;
        }
        regions_ptr = &regions;
    }

    if (expr.kind == Memmy_MemoryExprKind_Address)
    {
        Memmy_Addr address = 0;
        status = Memmy_MemoryExpr_ResolveAddress(process, modules_ptr, &expr, &address, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        String8 address_text = Memmy_Cli_FormatAddress(arena, pointer_width, address);
        if (options->json)
        {
            *out = String8_PushF(arena, "{\"address\":\"%.*s\"}\n", (int)address_text.len, (char *)address_text.data);
        }
        else
        {
            *out = String8_PushF(arena, "%.*s\n", (int)address_text.len, (char *)address_text.data);
        }
    }
    else if (expr.kind == Memmy_MemoryExprKind_Peek)
    {
        Memmy_ExecPeekResult result = {0};
        status = Memmy_MemoryExpr_ExecutePeek(arena, process, modules_ptr, &expr, &result, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_CliPeekOutput peek = {
            .pointer_width = pointer_width,
            .address = result.address,
            .type = result.type,
            .type_text = Memmy_Cli_TypeString(result.type),
            .bytes = result.value.bytes,
        };
        status = Memmy_Cli_FormatPeekOutput(arena, &peek, options->json, out, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    else if (expr.kind == Memmy_MemoryExprKind_Poke)
    {
        Memmy_ExecPokeResult result = {0};
        status = Memmy_MemoryExpr_ExecutePoke(arena, process, modules_ptr, &expr, &result, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_CliPokeOutput poke = {
            .pid = pid,
            .pointer_width = pointer_width,
            .address = result.address,
            .type = result.type,
            .type_text = Memmy_Cli_TypeString(result.type),
            .old_bytes = result.old_value.bytes,
            .new_bytes = result.new_value.bytes,
            .dry_run = 0,
        };
        status = Memmy_Cli_FormatPokeOutput(arena, &poke, options->json, out, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    else if (expr.kind == Memmy_MemoryExprKind_PatternScan)
    {
        Memmy_ScanResultList results = {0};
        status = Memmy_MemoryExpr_ExecutePatternScan(arena, process, modules_ptr, regions_ptr, &expr, &results, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Cli_FormatScanResults(arena, &results, pointer_width, options->json, options->jsonl);
    }
    else
    {
        Memmy_ScanResultList results = {0};
        status = Memmy_MemoryExpr_ExecuteValueScan(arena, process, modules_ptr, regions_ptr, &expr, &results, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Cli_FormatScanResults(arena, &results, pointer_width, options->json, options->jsonl);
    }
    return Memmy_Status_Ok;
}
