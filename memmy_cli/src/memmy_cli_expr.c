#include "memmy_cli_internal.h"

#include "memmy_exec.h"

static Memmy_Status Memmy_Cli_RejectNonExprOptions(Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options->has_filter || options->has_addr || options->has_type || options->has_count || options->has_value ||
        options->dry_run || options->has_start || options->has_end || options->has_length || options->has_limit ||
        options->has_chunk_size || options->has_pattern)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for --expr"), (String8){0});
    }
    return Memmy_Status_Ok;
}

static Memmy_ProcessSelector Memmy_Cli_ExprProcessSelector(Memmy_MemoryExpr *expr)
{
    Memmy_ProcessSelector selector = {0};
    if (expr->kind == Memmy_MemoryExprKind_Address && expr->address.base_kind == Memmy_AddressExprBaseKind_Target)
    {
        selector = expr->address.target.process;
    }
    return selector;
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
    if (expr.kind != Memmy_MemoryExprKind_Address)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("only address expressions are implemented"));
        return Memmy_Status_Unsupported;
    }

    Memmy_ExecRequirements requirements = {0};
    status = Memmy_MemoryExpr_GetRequirements(&expr, &requirements, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ProcessSelector expr_selector = Memmy_Cli_ExprProcessSelector(&expr);
    Memmy_BackendCap caps = requirements.backend_caps;
    if (options->has_name || expr_selector.kind == Memmy_ProcessSelectorKind_Name)
    {
        caps |= Memmy_BackendCap_ListProcs;
    }

    status = Memmy_Cli_RequireCaps(caps, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    B32 process_required = requirements.needs_external_process ||
                           expr_selector.kind != Memmy_ProcessSelectorKind_None || requirements.process_access != 0 ||
                           requirements.needs_modules || requirements.needs_regions;
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
        status = Memmy_Process_Open(arena, pid, requirements.process_access, &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        pointer_width = process->pointer_width;
    }

    Memmy_ModuleList modules = {0};
    Memmy_ModuleList *modules_ptr = 0;
    if (requirements.needs_modules)
    {
        status = Memmy_Process_ListModules(arena, process, &modules, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Process_Close(process);
            return status;
        }
        modules_ptr = &modules;
    }

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
    return Memmy_Status_Ok;
}
