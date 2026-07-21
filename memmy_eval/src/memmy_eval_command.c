#include "memmy_eval_internal.h"

#include "base.h"

static Memmy_Status MemmyEval_Result_Push(MemmyEval_ResultSink const *sink, MemmyEval_Result const *result)
{
    if (sink != 0 && sink->callback != 0)
    {
        return sink->callback(sink->user_data, result);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_ProcessEmitter_Push(void *user_data, Memmy_ProcessInfo const *info)
{
    MemmyEval_ProcessEmitter *emitter = (MemmyEval_ProcessEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(info->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    MemmyEval_Result result = {
        .kind = MemmyEval_ResultKind_Process,
        .process = *info,
    };
    return MemmyEval_Result_Push(emitter->sink, &result);
}

static Memmy_Status MemmyEval_ModuleEmitter_Push(void *user_data, Memmy_Module const *module)
{
    MemmyEval_ModuleEmitter *emitter = (MemmyEval_ModuleEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(module->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    MemmyEval_Result result = {
        .kind = MemmyEval_ResultKind_Module,
        .module = *module,
    };
    return MemmyEval_Result_Push(emitter->sink, &result);
}

static Memmy_Status MemmyEval_RegionEmitter_Push(void *user_data, Memmy_Region const *region)
{
    MemmyEval_RegionEmitter *emitter = (MemmyEval_RegionEmitter *)user_data;
    MemmyEval_Result result = {
        .kind = MemmyEval_ResultKind_Region,
        .region = *region,
    };
    return MemmyEval_Result_Push(emitter->sink, &result);
}

Memmy_Status MemmyEval_ValueResult_Emit(MemmyEval_ResultSink const *sink, Memmy_Value value)
{
    MemmyEval_Result result = {
        .kind = MemmyEval_ResultKind_Value,
        .value = value,
    };
    return MemmyEval_Result_Push(sink, &result);
}

Memmy_Status MemmyEval_Command_Eval(MemmyEval_Exec *exec, MemmyAst_Statement const *statement,
                                    MemmyEval_ResultSink const *sink, Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    if (statement->command_kind == MemmyAst_CommandKind_Procs)
    {
        MemmyEval_ProcessEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ProcessInfoSink process_sink = {
            .callback = MemmyEval_ProcessEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_Enumerate(exec->out_arena, process_sink, error);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Mods)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = MemmyEval_Process_Require(exec, String8_Lit("mods"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_ModuleEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ModuleSink module_sink = {
            .callback = MemmyEval_ModuleEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateModules(exec->out_arena, process, module_sink, error);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Regions)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = MemmyEval_Process_Require(exec, String8_Lit("regions"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_RegionEmitter emitter = {.sink = sink};
        Memmy_RegionSink region_sink = {
            .callback = MemmyEval_RegionEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateRegions(exec->out_arena, process, region_sink, error);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Vars)
    {
        HashMap_ForEach(MemmyEval_Binding, binding, &env->bindings, hash)
        {
            MemmyEval_Result result = {
                .kind = MemmyEval_ResultKind_Variable,
                .variable.name = String8_Copy(exec->out_arena, binding->name),
            };
            Memmy_Status status = MemmyEval_Env_Find(exec->out_arena, env, binding->name, &result.variable.value);
            if (status == Memmy_Status_Ok)
            {
                status = MemmyEval_Result_Push(sink, &result);
            }
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == MemmyAst_CommandKind_Unset)
    {
        Memmy_Status status = MemmyEval_Env_Unset(env, statement->command_arg);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        MemmyEval_Result result = {
            .kind = MemmyEval_ResultKind_Unset,
            .name = String8_Copy(exec->out_arena, statement->command_arg),
        };
        return MemmyEval_Result_Push(sink, &result);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Clear)
    {
        MemmyEval_Env_Clear(env);
        MemmyEval_Result result = {.kind = MemmyEval_ResultKind_Clear};
        return MemmyEval_Result_Push(sink, &result);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Help)
    {
        MemmyEval_Result result = {
            .kind = MemmyEval_ResultKind_Help,
            .text = String8_Lit("Core values:\n"
                                "  x                    constant integer/math expression\n"
                                "  nil                  type-neutral absence of a value\n"
                                "  @x                   absolute address\n"
                                "  [@a..@b]             explicit address range [a, b)\n"
                                "  [@a..+n]             sized address range [a, a+n)\n"
                                "  <module>             module range in selected process\n"
                                "  [0..]                selected process address-space range\n"
                                "  function address     function range containing address\n"
                                "  objectbase address   best-effort object base containing address\n"
                                "  $name                variable\n"
                                "  $rva = $hit - <module>  module-relative offset const\n"
                                "\n"
                                "Memory:\n"
                                "  range refs <ptr|rel32|any> address\n"
                                "  value |> expr        bind value to $ and evaluate expr once\n"
                                "  list => expr         filter-map address/range items\n"
                                "                       failed and nil RHS results are omitted\n"
                                "  $                    current flow input inside a flow RHS\n"
                                "  Flows chain left-to-right; parentheses nest; inner $ shadows outer $\n"
                                "  $matches |> $[0]\n"
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
                                "  /tutorial [hint|restart|stop]\n"
                                "  /exit\n"
                                "  /quit\n"),
        };
        return MemmyEval_Result_Push(sink, &result);
    }
    if (statement->command_kind == MemmyAst_CommandKind_Exit || statement->command_kind == MemmyAst_CommandKind_Quit)
    {
        MemmyEval_Result result = {.kind = MemmyEval_ResultKind_Exit};
        return MemmyEval_Result_Push(sink, &result);
    }

    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("command"), String8_Lit("unknown command"));
    return Memmy_Status_InvalidArgument;
}
