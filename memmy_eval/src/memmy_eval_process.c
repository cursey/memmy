#include "memmy_eval_internal.h"

#include "base.h"

void MemmyEval_Exec_Close(MemmyEval_Exec *exec)
{
    if (exec == 0)
    {
        return;
    }

    if (exec->process != 0)
    {
        Memmy_Process_Close(exec->process);
        exec->process = 0;
    }
    if (exec->process_arena != 0)
    {
        Arena_Destroy(exec->process_arena);
        exec->process_arena = 0;
    }
}

Memmy_Status MemmyEval_Exec_OpenProcess(MemmyEval_Exec *exec, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    if (exec == 0 || exec->env == 0 || out == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                            String8_Lit("missing eval execution context"));
        return Memmy_Status_InvalidArgument;
    }

    if (exec->process != 0)
    {
        if (exec->process->pid != pid)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                                String8_Lit("evaluation requested more than one process"));
            return Memmy_Status_InvalidArgument;
        }
        *out = exec->process;
        return Memmy_Status_Ok;
    }

    if (exec->process_arena == 0)
    {
        exec->process_arena = Arena_Create(Megabytes(1));
    }

    Memmy_Process *process = 0;
    Memmy_Status status = Memmy_Process_Open(exec->process_arena, pid, &process, error);
    if (status != Memmy_Status_Ok)
    {
        Arena_Clear(exec->process_arena);
        return status;
    }

    exec->process = process;
    *out = process;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Process_Require(MemmyEval_Exec *exec, String8 context, Memmy_Process **out, Memmy_Error *error)
{
    U32 pid = 0;
    if (exec != 0 && exec->env != 0 && exec->env->has_default_process)
    {
        pid = exec->env->default_pid;
    }
    else
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, context, String8_Lit("missing selected process"));
        return Memmy_Status_InvalidArgument;
    }

    return MemmyEval_Exec_OpenProcess(exec, pid, out, error);
}

static Memmy_Status MemmyEval_TargetProcess_Resolve(MemmyEval_Exec *exec, MemmyAst_Node const *target,
                                                    Memmy_Process **out, Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    (void)target;
    if (!env->has_default_process)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                            String8_Lit("missing selected process for target"));
        return Memmy_Status_InvalidArgument;
    }
    return MemmyEval_Exec_OpenProcess(exec, env->default_pid, out, error);
}

static Memmy_Status MemmyEval_ModuleResolver_Push(void *user_data, Memmy_Module const *module)
{
    MemmyEval_ModuleResolver *resolver = (MemmyEval_ModuleResolver *)user_data;
    if (!String8_EqNoCase(module->name, resolver->name))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        MemmyEval_Error_Set(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                            String8_Lit("module target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *module;
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Module_Resolve(MemmyEval_Exec *exec, MemmyAst_Node const *target, Memmy_Module *out,
                                             Memmy_Process **out_process, Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    Memmy_Process *process = 0;
    Memmy_Status status = MemmyEval_TargetProcess_Resolve(exec, target, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin((Arena *[]){env->arena, exec->out_arena}, 2);
    MemmyEval_ModuleResolver resolver = {
        .name = target->target_module,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = MemmyEval_ModuleResolver_Push,
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
        MemmyEval_Error_Set(error, Memmy_Status_NotFound, String8_Lit("target"),
                            String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = resolver.match;
    if (out_process != 0)
    {
        *out_process = process;
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Target_Eval(MemmyEval_Exec *exec, MemmyAst_Node const *target, Memmy_Value *out,
                                   Memmy_Error *error)
{
    if (target->target_module.len != 0)
    {
        Memmy_Module module = {0};
        Memmy_Status status = MemmyEval_Module_Resolve(exec, target, &module, 0, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Range_FromStartLength(module.base, module.size, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_Value){.type = Memmy_Type_Range, .range = range};
        return Memmy_Status_Ok;
    }

    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"), String8_Lit("empty target"));
    return Memmy_Status_InvalidArgument;
}

Memmy_Status MemmyEval_Expr_EvalProcess(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out,
                                        Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    (void)env;
    if (expr->kind == MemmyAst_NodeKind_Target)
    {
        return MemmyEval_Target_Eval(exec, expr, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_ProcessRange)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = MemmyEval_Process_Require(exec, String8_Lit("range"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Process_GetAddressRange(process, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_Value){.type = Memmy_Type_Range, .range = range};
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Function)
    {
        Memmy_Value value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = MemmyEval_Value_AsAddress(&value, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, String8_Lit("function"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Process_FindFunction(exec->out_arena, process, address, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_Value){.type = Memmy_Type_Range, .range = range};
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ObjectBase)
    {
        Memmy_Value value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = MemmyEval_Value_AsAddress(&value, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Process *process = 0;
        status = MemmyEval_Process_Require(exec, String8_Lit("objectbase"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_ObjectBaseResult result = {0};
        status = Memmy_Process_FindObjectBase(exec->out_arena, process, address, 0, &result, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_Value){.type = Memmy_Type_Address, .address = result.address};
        return Memmy_Status_Ok;
    }
    MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
