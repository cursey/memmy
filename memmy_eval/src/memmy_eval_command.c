#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

static Memmy_Status Memmy_EvalResult_Push(Memmy_EvalResultSink const *sink, Memmy_EvalResult const *result)
{
    if (sink != 0 && sink->callback != 0)
    {
        return sink->callback(sink->user_data, result);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EvalProcessEmitter_Push(void *user_data, Memmy_ProcessInfo const *info)
{
    Memmy_EvalProcessEmitter *emitter = (Memmy_EvalProcessEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(info->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    Memmy_EvalResult result = {
        .kind = Memmy_EvalResultKind_Process,
        .process = *info,
    };
    return Memmy_EvalResult_Push(emitter->sink, &result);
}

static Memmy_Status Memmy_EvalModuleEmitter_Push(void *user_data, Memmy_Module const *module)
{
    Memmy_EvalModuleEmitter *emitter = (Memmy_EvalModuleEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(module->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    Memmy_EvalResult result = {
        .kind = Memmy_EvalResultKind_Module,
        .module = *module,
    };
    return Memmy_EvalResult_Push(emitter->sink, &result);
}

static Memmy_Status Memmy_EvalRegionEmitter_Push(void *user_data, Memmy_Region const *region)
{
    Memmy_EvalRegionEmitter *emitter = (Memmy_EvalRegionEmitter *)user_data;
    Memmy_EvalResult result = {
        .kind = Memmy_EvalResultKind_Region,
        .region = *region,
    };
    return Memmy_EvalResult_Push(emitter->sink, &result);
}

static Memmy_EvalResultKind Memmy_EvalResultKind_ForValue(Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_TypedValue && value.old_typed_value.bytes.data != 0)
    {
        return Memmy_EvalResultKind_Write;
    }
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        return Memmy_EvalResultKind_Read;
    }
    if (value.kind == Memmy_EvalValueKind_AddressList)
    {
        return Memmy_EvalResultKind_AddressList;
    }
    return Memmy_EvalResultKind_Value;
}

Memmy_Status Memmy_Eval_EmitValueResult(Memmy_EvalResultSink const *sink, Memmy_EvalValue value)
{
    Memmy_EvalResult result = {
        .kind = Memmy_EvalResultKind_ForValue(value),
        .value = value,
    };
    if (result.kind == Memmy_EvalResultKind_Write)
    {
        result.address = value.address;
        result.old_value = value.old_typed_value;
        result.new_value = value.typed_value;
    }
    else if (result.kind == Memmy_EvalResultKind_Read)
    {
        result.address = value.address;
        result.new_value = value.typed_value;
    }
    return Memmy_EvalResult_Push(sink, &result);
}

Memmy_Status Memmy_Eval_Command(Memmy_EvalExec *exec, Memmy_AstStatement const *statement,
                                Memmy_EvalResultSink const *sink, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    if (statement->command_kind == Memmy_AstCommandKind_Procs)
    {
        Memmy_EvalProcessEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ProcessInfoSink process_sink = {
            .callback = Memmy_EvalProcessEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_EnumerateProcesses(exec->out_arena, process_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Mods)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("mods"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalModuleEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ModuleSink module_sink = {
            .callback = Memmy_EvalModuleEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateModules(exec->out_arena, process, module_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Regions)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("regions"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalRegionEmitter emitter = {.sink = sink};
        Memmy_RegionSink region_sink = {
            .callback = Memmy_EvalRegionEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateRegions(exec->out_arena, process, region_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Vars)
    {
        HashMap_ForEach(Memmy_EvalBinding, binding, &env->bindings, hash)
        {
            Memmy_EvalResult result = {
                .kind = Memmy_EvalResultKind_Variable,
                .variable.name = String8_Copy(exec->out_arena, binding->name),
            };
            Memmy_Status status = Memmy_EvalEnv_Find(exec->out_arena, env, binding->name, &result.variable.value);
            if (status == Memmy_Status_Ok)
            {
                status = Memmy_EvalResult_Push(sink, &result);
            }
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == Memmy_AstCommandKind_Unset)
    {
        Memmy_Status status = Memmy_EvalEnv_Unset(env, statement->command_arg);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_EvalResult result = {
            .kind = Memmy_EvalResultKind_Unset,
            .name = String8_Copy(exec->out_arena, statement->command_arg),
        };
        return Memmy_EvalResult_Push(sink, &result);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Clear)
    {
        Memmy_EvalEnv_Clear(env);
        Memmy_EvalResult result = {.kind = Memmy_EvalResultKind_Clear};
        return Memmy_EvalResult_Push(sink, &result);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Help)
    {
        Memmy_EvalResult result = {
            .kind = Memmy_EvalResultKind_Help,
            .text = String8_Lit("Core values:\n"
                                "  x                    constant integer/math expression\n"
                                "  @x                   absolute address\n"
                                "  [@a..@b]             explicit address range [a, b)\n"
                                "  [@a..+n]             sized address range [a, a+n)\n"
                                "  <module>             module range in selected process\n"
                                "  [0..]                selected process readable regions\n"
                                "  function address     function range containing address\n"
                                "  objectbase address   best-effort object base containing address\n"
                                "  $name                variable\n"
                                "  $rva = $hit - <module>  module-relative offset const\n"
                                "\n"
                                "Memory:\n"
                                "  range refs <ptr|rel32|any> address\n"
                                "  list => expr         transform each address/range item\n"
                                "  $                    current item inside transform RHS\n"
                                "  $matches => [$..+0x20]\n"
                                "  $fn = function $xrefs[0]\n"
                                "  $hits => objectbase $\n"
                                "  $ranges => $ + 4\n"
                                "  $name[N]             index address/range list\n"
                                "\n"
                                "Commands:\n"
                                "  /procs [filter]\n"
                                "  /attach <pid|name>  select process and clear variables\n"
                                "  /detach             clear selected process and variables\n"
                                "  /mods [filter]\n"
                                "  /regions\n"
                                "  /vars\n"
                                "  /unset $var\n"
                                "  /clear\n"
                                "  /help\n"
                                "  /exit\n"
                                "  /quit\n"),
        };
        return Memmy_EvalResult_Push(sink, &result);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Exit || statement->command_kind == Memmy_AstCommandKind_Quit)
    {
        Memmy_EvalResult result = {.kind = Memmy_EvalResultKind_Exit};
        return Memmy_EvalResult_Push(sink, &result);
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("command"), String8_Lit("unknown command"));
    return Memmy_Status_InvalidArgument;
}
