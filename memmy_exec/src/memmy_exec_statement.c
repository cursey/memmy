#include "memmy_exec.h"

typedef struct Memmy_ExecExprNeeds Memmy_ExecExprNeeds;
struct Memmy_ExecExprNeeds
{
    B32 process;
};

static Memmy_Status Memmy_ExecResult_Emit(Memmy_ExecResultSink sink, Memmy_ExecResult *result, Memmy_Error *error)
{
    if (sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("exec"), String8_Lit("missing result sink"));
        return Memmy_Status_InvalidArgument;
    }
    return sink.callback(sink.user_data, result);
}

static void Memmy_Exec_TargetExprNeeds(Memmy_TargetExpr *target, Memmy_ExecExprNeeds *needs)
{
    if (target->kind == Memmy_TargetExprKind_Module || target->kind == Memmy_TargetExprKind_WholeProcess)
    {
        needs->process = 1;
    }
}

static void Memmy_Exec_AddressExprNeeds(Memmy_ExecEnv *env, Memmy_AddressExpr *address, Memmy_ExecExprNeeds *needs);
static void Memmy_Exec_RangeExprNeeds(Memmy_ExecEnv *env, Memmy_RangeExpr *range, Memmy_ExecExprNeeds *needs);

static void Memmy_Exec_AddressExprNeeds(Memmy_ExecEnv *env, Memmy_AddressExpr *address, Memmy_ExecExprNeeds *needs)
{
    if (address->base_kind == Memmy_AddressExprBaseKind_Target)
    {
        Memmy_Exec_TargetExprNeeds(&address->target, needs);
    }
    else if (address->base_kind == Memmy_AddressExprBaseKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, address->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Address)
        {
            if (Memmy_ExecEnv_ResolvePush(env, address->variable.name, 0) == Memmy_Status_Ok)
            {
                Memmy_Exec_AddressExprNeeds(env, &Memmy_ExecVariableBinding_Expr(binding)->address, needs);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }

    List_ForEach(Memmy_AddressOp, op, &address->ops, link)
    {
        if (op->kind == Memmy_AddressOpKind_Deref || op->kind == Memmy_AddressOpKind_DerefOffset)
        {
            needs->process = 1;
        }
    }
}

static void Memmy_Exec_RangeExprNeeds(Memmy_ExecEnv *env, Memmy_RangeExpr *range, Memmy_ExecExprNeeds *needs)
{
    if (range->kind == Memmy_RangeExprKind_Target || range->kind == Memmy_RangeExprKind_ModuleOffset ||
        range->kind == Memmy_RangeExprKind_ModuleSized)
    {
        Memmy_Exec_TargetExprNeeds(&range->target, needs);
    }
    else if (range->kind == Memmy_RangeExprKind_AddressSized)
    {
        Memmy_Exec_AddressExprNeeds(env, &range->address, needs);
    }
    else if (range->kind == Memmy_RangeExprKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, range->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Range)
        {
            if (Memmy_ExecEnv_ResolvePush(env, range->variable.name, 0) == Memmy_Status_Ok)
            {
                Memmy_Exec_RangeExprNeeds(env, &Memmy_ExecVariableBinding_Expr(binding)->range, needs);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }
}

static Memmy_ExecExprNeeds Memmy_Exec_MemoryExprNeeds(Memmy_ExecEnv *env, Memmy_MemoryExpr *expr)
{
    Memmy_ExecExprNeeds needs = {0};
    if (expr->kind == Memmy_MemoryExprKind_Address)
    {
        Memmy_Exec_AddressExprNeeds(env, &expr->address, &needs);
    }
    else if (expr->kind == Memmy_MemoryExprKind_Peek || expr->kind == Memmy_MemoryExprKind_Poke)
    {
        needs.process = 1;
        Memmy_Exec_AddressExprNeeds(env, &expr->address, &needs);
    }
    else if (expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        needs.process = 1;
        Memmy_Exec_RangeExprNeeds(env, &expr->range, &needs);
    }
    return needs;
}

static Memmy_ProcessSelector Memmy_Exec_AddressExprProcessSelector(Memmy_ExecEnv *env, Memmy_AddressExpr *expr);
static Memmy_ProcessSelector Memmy_Exec_RangeExprProcessSelector(Memmy_ExecEnv *env, Memmy_RangeExpr *expr);

static Memmy_ProcessSelector Memmy_Exec_AddressExprProcessSelector(Memmy_ExecEnv *env, Memmy_AddressExpr *expr)
{
    Memmy_ProcessSelector selector = {0};
    if (expr->base_kind == Memmy_AddressExprBaseKind_Target)
    {
        selector = expr->target.process;
    }
    else if (expr->base_kind == Memmy_AddressExprBaseKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, expr->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Address)
        {
            if (Memmy_ExecEnv_ResolvePush(env, expr->variable.name, 0) == Memmy_Status_Ok)
            {
                selector =
                    Memmy_Exec_AddressExprProcessSelector(env, &Memmy_ExecVariableBinding_Expr(binding)->address);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }
    return selector;
}

static Memmy_ProcessSelector Memmy_Exec_RangeExprProcessSelector(Memmy_ExecEnv *env, Memmy_RangeExpr *expr)
{
    Memmy_ProcessSelector selector = {0};
    if (expr->kind == Memmy_RangeExprKind_Target || expr->kind == Memmy_RangeExprKind_ModuleOffset ||
        expr->kind == Memmy_RangeExprKind_ModuleSized)
    {
        selector = expr->target.process;
    }
    else if (expr->kind == Memmy_RangeExprKind_AddressSized)
    {
        selector = Memmy_Exec_AddressExprProcessSelector(env, &expr->address);
    }
    else if (expr->kind == Memmy_RangeExprKind_Variable && env != 0)
    {
        Memmy_ExecVariableBinding *binding = 0;
        if (Memmy_ExecEnv_Find(env, expr->variable.name, &binding, 0) == Memmy_Status_Ok &&
            Memmy_ExecVariableBinding_Kind(binding) == Memmy_VariableExprKind_Range)
        {
            if (Memmy_ExecEnv_ResolvePush(env, expr->variable.name, 0) == Memmy_Status_Ok)
            {
                selector = Memmy_Exec_RangeExprProcessSelector(env, &Memmy_ExecVariableBinding_Expr(binding)->range);
                Memmy_ExecEnv_ResolvePop(env);
            }
        }
    }
    return selector;
}

static Memmy_ProcessSelector Memmy_Exec_MemoryExprProcessSelector(Memmy_ExecEnv *env, Memmy_MemoryExpr *expr)
{
    Memmy_ProcessSelector selector = {0};
    if ((expr->kind == Memmy_MemoryExprKind_Address || expr->kind == Memmy_MemoryExprKind_Peek ||
         expr->kind == Memmy_MemoryExprKind_Poke))
    {
        selector = Memmy_Exec_AddressExprProcessSelector(env, &expr->address);
    }
    else if (expr->kind == Memmy_MemoryExprKind_PatternScan || expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        selector = Memmy_Exec_RangeExprProcessSelector(env, &expr->range);
    }
    return selector;
}

typedef struct Memmy_ExecProcessNameResolver Memmy_ExecProcessNameResolver;
struct Memmy_ExecProcessNameResolver
{
    String8 name;
    U32 match_pid;
    U64 match_count;
};

static Memmy_Status Memmy_ExecProcessNameResolver_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_ExecProcessNameResolver *resolver = (Memmy_ExecProcessNameResolver *)user_data;
    if (String8_EqNoCase(info->name, resolver->name))
    {
        resolver->match_pid = info->pid;
        resolver->match_count++;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ResolveProcessName(Arena *arena, String8 name, U32 *out_pid, Memmy_Error *error)
{
    Memmy_ExecProcessNameResolver resolver = {
        .name = name,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = Memmy_ExecProcessNameResolver_Push,
        .user_data = &resolver,
    };
    Memmy_Status status = Memmy_EnumerateProcesses(arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (resolver.match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("process"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (resolver.match_count > 1)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("process"),
                        String8_Lit("process name is ambiguous"));
        return Memmy_Status_Ambiguous;
    }

    *out_pid = resolver.match_pid;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ResolveProcessSelector(Arena *arena, Memmy_ProcessSelector selector, U32 *out_pid,
                                                      Memmy_Error *error)
{
    if (selector.kind == Memmy_ProcessSelectorKind_Pid)
    {
        *out_pid = selector.pid;
        return Memmy_Status_Ok;
    }
    if (selector.kind == Memmy_ProcessSelectorKind_Name)
    {
        return Memmy_Exec_ResolveProcessName(arena, selector.name, out_pid, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                    String8_Lit("missing process selector"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Exec_SelectProcess(Arena *arena, Memmy_ExecProcessSelection selection,
                                             Memmy_ProcessSelector expr_selector, B32 process_required, U32 *out_pid,
                                             Memmy_Error *error)
{
    if (selection.has_pid && selection.has_name)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"),
                        String8_Lit("use --pid or --name, not both"));
        return Memmy_Status_ParseError;
    }

    B32 has_external = selection.has_pid || selection.has_name;
    U32 external_pid = 0;
    if (selection.has_pid)
    {
        external_pid = selection.pid;
    }
    else if (selection.has_name)
    {
        Memmy_Status status = Memmy_Exec_ResolveProcessName(arena, selection.name, &external_pid, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    B32 has_expr_selector = expr_selector.kind != Memmy_ProcessSelectorKind_None;
    U32 expr_pid = 0;
    if (has_expr_selector)
    {
        Memmy_Status status = Memmy_Exec_ResolveProcessSelector(arena, expr_selector, &expr_pid, error);
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

typedef struct Memmy_ExecStatementScanSink Memmy_ExecStatementScanSink;
struct Memmy_ExecStatementScanSink
{
    Memmy_ExecResultSink sink;
    Memmy_PointerWidth pointer_width;
    U64 match_count;
};

static Memmy_Status Memmy_ExecStatementScanSink_Push(void *user_data, Memmy_Addr address)
{
    Memmy_ExecStatementScanSink *scan_sink = (Memmy_ExecStatementScanSink *)user_data;
    scan_sink->match_count++;
    Memmy_ExecResult result = {
        .kind = Memmy_ExecResultKind_Match,
        .match =
            {
                .address = address,
                .pointer_width = scan_sink->pointer_width,
            },
    };
    return scan_sink->sink.callback(scan_sink->sink.user_data, &result);
}

static Memmy_Status Memmy_Statement_ExecuteMemory(Arena *arena, Memmy_ExecEnv *env, Memmy_MemoryExpr *expr,
                                                  Memmy_ExecProcessSelection selection, Memmy_ExecResultSink sink,
                                                  Memmy_Error *error)
{
    Memmy_ExecExprNeeds needs = Memmy_Exec_MemoryExprNeeds(env, expr);
    Memmy_ProcessSelector expr_selector = Memmy_Exec_MemoryExprProcessSelector(env, expr);
    B32 process_required = needs.process || expr_selector.kind != Memmy_ProcessSelectorKind_None;
    U32 pid = 0;
    Memmy_Status status = Memmy_Exec_SelectProcess(arena, selection, expr_selector, process_required, &pid, error);
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

    if (expr->kind == Memmy_MemoryExprKind_Address)
    {
        Memmy_Addr address = 0;
        status = Memmy_MemoryExpr_ResolveAddressWithEnv(env, process, expr, &address, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_ExecResult result = {
            .kind = Memmy_ExecResultKind_Address,
            .address =
                {
                    .address = address,
                    .pointer_width = pointer_width,
                },
        };
        return Memmy_ExecResult_Emit(sink, &result, error);
    }

    if (expr->kind == Memmy_MemoryExprKind_Peek)
    {
        Memmy_ExecResult result = {.kind = Memmy_ExecResultKind_Peek};
        status = Memmy_MemoryExpr_ExecutePeekWithEnv(arena, env, process, expr, &result.peek, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_ExecResult_Emit(sink, &result, error);
    }

    if (expr->kind == Memmy_MemoryExprKind_Poke)
    {
        Memmy_ExecResult result = {.kind = Memmy_ExecResultKind_Poke};
        status = Memmy_MemoryExpr_ExecutePokeWithEnv(arena, env, process, expr, &result.poke, error);
        if (process != 0)
        {
            Memmy_Process_Close(process);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_ExecResult_Emit(sink, &result, error);
    }

    Memmy_ExecStatementScanSink scan_sink = {
        .sink = sink,
        .pointer_width = pointer_width,
    };
    Memmy_ScanSink memory_sink = {
        .callback = Memmy_ExecStatementScanSink_Push,
        .user_data = &scan_sink,
    };
    if (expr->kind == Memmy_MemoryExprKind_PatternScan)
    {
        status = Memmy_MemoryExpr_ExecutePatternScanWithEnv(arena, env, process, expr, memory_sink, error);
    }
    else if (expr->kind == Memmy_MemoryExprKind_ValueScan)
    {
        status = Memmy_MemoryExpr_ExecuteValueScanWithEnv(arena, env, process, expr, memory_sink, error);
    }
    else
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("only address, peek, poke, and scan expressions are implemented"));
        status = Memmy_Status_Unsupported;
    }

    if (process != 0)
    {
        Memmy_Process_Close(process);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ExecResult summary = {
        .kind = Memmy_ExecResultKind_Summary,
        .summary =
            {
                .match_count = scan_sink.match_count,
            },
    };
    return Memmy_ExecResult_Emit(sink, &summary, error);
}

typedef struct Memmy_ExecProcessEmitter Memmy_ExecProcessEmitter;
struct Memmy_ExecProcessEmitter
{
    Memmy_ExecResultSink sink;
};

static Memmy_Status Memmy_ExecProcessEmitter_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_ExecProcessEmitter *emitter = (Memmy_ExecProcessEmitter *)user_data;
    Memmy_ExecResult result = {
        .kind = Memmy_ExecResultKind_Process,
        .process =
            {
                .info = *info,
            },
    };
    return emitter->sink.callback(emitter->sink.user_data, &result);
}

Memmy_Status Memmy_Statement_ExecuteWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Statement *statement,
                                            Memmy_ExecProcessSelection selection, Memmy_ExecResultSink sink,
                                            Memmy_Error *error)
{
    if (arena == 0 || env == 0 || statement == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("exec"),
                        String8_Lit("missing arena, environment, statement, or result sink"));
        return Memmy_Status_InvalidArgument;
    }

    if (statement->kind == Memmy_StatementKind_Memory)
    {
        return Memmy_Statement_ExecuteMemory(arena, env, &statement->memory, selection, sink, error);
    }
    if (statement->kind == Memmy_StatementKind_Procs)
    {
        Memmy_ExecProcessEmitter emitter = {
            .sink = sink,
        };
        Memmy_ProcessInfoSink process_sink = {
            .callback = Memmy_ExecProcessEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_EnumerateProcesses(arena, process_sink, error);
    }
    if (statement->kind == Memmy_StatementKind_Assignment)
    {
        Memmy_Status status = Memmy_ExecEnv_Set(env, statement->variable.name, &statement->assignment, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_ExecResult result = {
            .kind = Memmy_ExecResultKind_Assignment,
            .assignment =
                {
                    .name = statement->variable.name,
                    .variable_kind = statement->assignment.kind,
                    .status = Memmy_Status_Ok,
                },
        };
        return Memmy_ExecResult_Emit(sink, &result, error);
    }
    if (statement->kind == Memmy_StatementKind_Vars)
    {
        for (Memmy_ExecVariableBinding *binding = Memmy_ExecEnv_First(env); binding != 0;
             binding = Memmy_ExecEnv_Next(env, binding))
        {
            Memmy_ExecResult result = {
                .kind = Memmy_ExecResultKind_VariableBinding,
                .variable_binding =
                    {
                        .name = Memmy_ExecVariableBinding_Name(binding),
                        .variable_kind = Memmy_ExecVariableBinding_Kind(binding),
                        .status = Memmy_Status_Ok,
                    },
            };
            Memmy_Status status = Memmy_ExecResult_Emit(sink, &result, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        return Memmy_Status_Ok;
    }
    if (statement->kind == Memmy_StatementKind_Unset)
    {
        Memmy_Status status = Memmy_ExecEnv_Unset(env, statement->variable.name, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_ExecResult result = {
            .kind = Memmy_ExecResultKind_Unset,
            .unset =
                {
                    .name = statement->variable.name,
                    .status = Memmy_Status_Ok,
                },
        };
        return Memmy_ExecResult_Emit(sink, &result, error);
    }
    if (statement->kind == Memmy_StatementKind_Exit)
    {
        Memmy_ExecResult result = {
            .kind = Memmy_ExecResultKind_Control,
            .control =
                {
                    .kind = Memmy_ExecControlKind_Exit,
                },
        };
        return Memmy_ExecResult_Emit(sink, &result, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                    String8_Lit("unknown statement kind"));
    return Memmy_Status_InvalidArgument;
}

Memmy_Status Memmy_Statement_Execute(Arena *arena, Memmy_Statement *statement, Memmy_ExecProcessSelection selection,
                                     Memmy_ExecResultSink sink, Memmy_Error *error)
{
    if (arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("exec"), String8_Lit("missing arena"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    return Memmy_Statement_ExecuteWithEnv(arena, &env, statement, selection, sink, error);
}
