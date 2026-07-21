#include "memmy_eval_internal.h"

#include "base.h"

static B32 MemmyEval_Binding_Eq(void *link, void *ctx)
{
    MemmyEval_Binding *binding = ContainerOf((HashLink *)link, MemmyEval_Binding, hash);
    String8 *name = (String8 *)ctx;
    return String8_Eq(binding->name, *name);
}

static MemmyEval_Binding *MemmyEval_Env_FindBinding(MemmyEval_Env const *env, String8 name)
{
    U64 hash = Hash_Fnv1a(name.data, name.len);
    HashLink *link = HashMap_Find((HashMap *)&env->bindings, hash, MemmyEval_Binding_Eq, &name);
    return link != 0 ? ContainerOf(link, MemmyEval_Binding, hash) : 0;
}

MemmyEval_Env *MemmyEval_Env_Create(Arena *arena)
{
    if (arena == 0)
    {
        return 0;
    }

    MemmyEval_Env *env = Arena_PushStruct(arena, MemmyEval_Env);
    env->arena = arena;
    env->bindings = HashMap_Create(arena);
    return env;
}

void MemmyEval_Env_SetDefaultProcess(MemmyEval_Env *env, U32 pid, Memmy_PointerWidth pointer_width)
{
    if (env != 0)
    {
        env->has_default_process = 1;
        env->default_pid = pid;
        env->default_pointer_width = pointer_width;
    }
}

void MemmyEval_Env_ClearDefaultProcess(MemmyEval_Env *env)
{
    if (env != 0)
    {
        env->has_default_process = 0;
        env->default_pid = 0;
        env->default_pointer_width = Memmy_PointerWidth_Unknown;
    }
}

B32 MemmyEval_Env_GetDefaultProcess(MemmyEval_Env const *env, U32 *out_pid, Memmy_PointerWidth *out_pointer_width)
{
    if (out_pid != 0)
    {
        *out_pid = 0;
    }
    if (out_pointer_width != 0)
    {
        *out_pointer_width = Memmy_PointerWidth_Unknown;
    }
    if (env == 0 || !env->has_default_process)
    {
        return 0;
    }
    if (out_pid != 0)
    {
        *out_pid = env->default_pid;
    }
    if (out_pointer_width != 0)
    {
        *out_pointer_width = env->default_pointer_width;
    }
    return 1;
}

Memmy_Status MemmyEval_Env_Set(MemmyEval_Env *env, String8 name, Memmy_Value value)
{
    if (env == 0 || name.len == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Binding *binding = MemmyEval_Env_FindBinding(env, name);
    if (binding == 0)
    {
        binding = Arena_PushStruct(env->arena, MemmyEval_Binding);
        binding->name = String8_Copy(env->arena, name);
        binding->hash.hash = Hash_Fnv1a(binding->name.data, binding->name.len);
        HashMap_Insert(&env->bindings, &binding->hash);
    }
    return Memmy_Value_Copy(env->arena, &value, &binding->value, 0);
}

Memmy_Status MemmyEval_Env_Find(Arena *out_arena, MemmyEval_Env const *env, String8 name, Memmy_Value *out)
{
    if (out != 0)
    {
        *out = (Memmy_Value){0};
    }
    if (out_arena == 0 || env == 0 || out == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Binding *binding = MemmyEval_Env_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }

    return Memmy_Value_Copy(out_arena, &binding->value, out, 0);
}

Memmy_Status MemmyEval_Env_TypeFind(MemmyEval_Env const *env, String8 name, Memmy_Type *out)
{
    if (out != 0)
    {
        *out = (Memmy_Type){0};
    }
    if (env == 0 || out == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Binding *binding = MemmyEval_Env_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }
    *out = binding->value.type;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Env_Unset(MemmyEval_Env *env, String8 name)
{
    if (env == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Binding *binding = MemmyEval_Env_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }
    HashMap_Remove(&env->bindings, &binding->hash);
    return Memmy_Status_Ok;
}

void MemmyEval_Env_Clear(MemmyEval_Env *env)
{
    if (env != 0)
    {
        env->bindings = HashMap_Create(env->arena);
    }
}
