#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

void Memmy_EvalExec_Close(Memmy_EvalExec *exec)
{
    if (exec == 0)
    {
        return;
    }

    List_ForEach(Memmy_EvalOpenProcess, node, &exec->open_processes, link)
    {
        Memmy_Process_Close(node->process);
    }
}

Memmy_Status Memmy_EvalExec_OpenProcess(Memmy_EvalExec *exec, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    if (exec == 0 || exec->env == 0 || out == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                        String8_Lit("missing eval execution context"));
        return Memmy_Status_InvalidArgument;
    }

    List_ForEach(Memmy_EvalOpenProcess, node, &exec->open_processes, link)
    {
        if (node->process != 0 && node->process->pid == pid)
        {
            *out = node->process;
            return Memmy_Status_Ok;
        }
    }

    Memmy_Process *process = 0;
    Arena *arena = exec->transient_arena != 0 ? exec->transient_arena : exec->env->arena;
    Memmy_Status status = Memmy_Process_Open(arena, pid, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_EvalOpenProcess *node = Arena_PushStruct(arena, Memmy_EvalOpenProcess);
    node->process = process;
    List_PushBack(&exec->open_processes, &node->link);
    *out = process;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_RequireProcess(Memmy_EvalExec *exec, Memmy_EvalValue *value, String8 context,
                                       Memmy_Process **out, Memmy_Error *error)
{
    (void)value;
    U32 pid = 0;
    if (exec != 0 && exec->env != 0 && exec->env->has_default_process)
    {
        pid = exec->env->default_pid;
    }
    else
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, context, String8_Lit("missing selected process"));
        return Memmy_Status_InvalidArgument;
    }

    return Memmy_EvalExec_OpenProcess(exec, pid, out, error);
}

static Memmy_Status Memmy_Eval_ResolveTargetProcess(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_Process **out,
                                                    Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)target;
    if (!env->has_default_process)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing selected process for target"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_EvalExec_OpenProcess(exec, env->default_pid, out, error);
}

static Memmy_Status Memmy_EvalModuleResolver_Push(void *user_data, Memmy_Module *module)
{
    Memmy_EvalModuleResolver *resolver = (Memmy_EvalModuleResolver *)user_data;
    if (!String8_EqNoCase(module->name, resolver->name))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        Memmy_EvalError(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                        String8_Lit("module target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *module;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ResolveModule(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_Module *out,
                                             Memmy_Process **out_process, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    Memmy_Process *process = 0;
    Memmy_Status status = Memmy_Eval_ResolveTargetProcess(exec, target, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(&env->arena, 1);
    Memmy_EvalModuleResolver resolver = {
        .name = target->target_module,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_EvalModuleResolver_Push,
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
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = resolver.match;
    if (out_process != 0)
    {
        *out_process = process;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_Target(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_EvalValue *out, Memmy_Error *error)
{
    if (target->target_module.len != 0)
    {
        Memmy_Module module = {0};
        Memmy_Status status = Memmy_Eval_ResolveModule(exec, target, &module, 0, error);
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
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = range};
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("target"), String8_Lit("empty target"));
    return Memmy_Status_InvalidArgument;
}

Memmy_Status Memmy_Eval_ProcessExpr(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)env;
    if (expr->kind == Memmy_AstNodeKind_Target)
    {
        return Memmy_Eval_Target(exec, expr, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_ProcessRange)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("range"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        (void)process;
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_ProcessRange};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Function)
    {
        Memmy_EvalValue value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&value, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &value, String8_Lit("function"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Process_FindFunction(env->arena, process, address, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = range};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ObjectBase)
    {
        Memmy_EvalValue value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&value, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &value, String8_Lit("objectbase"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_ObjectBaseResult result = {0};
        status = Memmy_Process_FindObjectBase(env->arena, process, address, 0, &result, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = result.address};
        return Memmy_Status_Ok;
    }
    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
