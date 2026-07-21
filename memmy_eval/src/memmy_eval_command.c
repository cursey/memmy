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

Memmy_Status MemmyEval_Result_EmitValue(MemmyEval_ResultSink const *sink, Memmy_Value value)
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
            .text = String8_Lit("Values and types:\n"
                                "  42 / 42.5 / \"text\"  i64, f64, and str literals\n"
                                "  nil                    null value\n"
                                "  value as T             scalar conversion\n"
                                "  @integer               integer-to-address construction\n"
                                "  [@a..@b] / [@a..+n]   half-open ranges\n"
                                "  <module> / [0..]       selected-process ranges\n"
                                "  Types: u8 i8 u16 i16 u32 i32 u64 i64 f32 f64 str wstr\n"
                                "\n"
                                "Memory and scans:\n"
                                "  address as T           typed read\n"
                                "  address->offset        pointer read and optional offset\n"
                                "  range{pattern}         raw pattern scan\n"
                                "  range as T == expr     converted value scan\n"
                                "  range refs <ptr|rel32|any> address\n"
                                "  range disasm x64 {...}\n"
                                "\n"
                                "Lists and flows:\n"
                                "  list[N]                index any homogeneous list\n"
                                "  value |> expr          bind the whole value to $ once\n"
                                "  list => expr           filter-map any list; flatten list results\n"
                                "                         failed and nil item results are omitted\n"
                                "  Typed empty results retain list<T>; nil => expr stays nil\n"
                                "  $matches |> $[0]\n"
                                "  $values => $ as u16\n"
                                "  $xrefs => function $\n"
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
