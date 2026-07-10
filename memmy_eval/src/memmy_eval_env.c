#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

static B32 Memmy_EvalBinding_Eq(void *link, void *ctx)
{
    Memmy_EvalBinding *binding = ContainerOf((HashLink *)link, Memmy_EvalBinding, hash);
    String8 *name = (String8 *)ctx;
    return String8_Eq(binding->name, *name);
}

static Memmy_EvalBinding *Memmy_EvalEnv_FindBinding(Memmy_EvalEnv const *env, String8 name)
{
    U64 hash = Hash_Fnv1a(name.data, name.len);
    HashLink *link = HashMap_Find((HashMap *)&env->bindings, hash, Memmy_EvalBinding_Eq, &name);
    return link != 0 ? ContainerOf(link, Memmy_EvalBinding, hash) : 0;
}

static Memmy_EvalValue Memmy_EvalValue_Copy(Arena *arena, Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        value.typed_value.bytes = String8_Copy(arena, value.typed_value.bytes);
        value.old_typed_value.bytes = String8_Copy(arena, value.old_typed_value.bytes);
    }
    if (value.kind == Memmy_EvalValueKind_AddressList && value.address_count != 0)
    {
        Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, value.address_count);
        Memory_Copy(addresses, value.addresses, sizeof(addresses[0]) * value.address_count);
        value.addresses = addresses;
    }
    if (value.kind == Memmy_EvalValueKind_RangeList && value.range_count != 0)
    {
        Memmy_Range *ranges = Arena_PushArrayNoZero(arena, Memmy_Range, value.range_count);
        Memory_Copy(ranges, value.ranges, sizeof(ranges[0]) * value.range_count);
        value.ranges = ranges;
    }
    return value;
}

Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena)
{
    if (arena == 0)
    {
        return 0;
    }

    Memmy_EvalEnv *env = Arena_PushStruct(arena, Memmy_EvalEnv);
    env->arena = arena;
    env->bindings = HashMap_Create(arena);
    return env;
}

void Memmy_EvalEnv_SetDefaultProcess(Memmy_EvalEnv *env, U32 pid, Memmy_PointerWidth pointer_width)
{
    if (env != 0)
    {
        env->has_default_process = 1;
        env->default_pid = pid;
        env->default_pointer_width = pointer_width;
    }
}

void Memmy_EvalEnv_ClearDefaultProcess(Memmy_EvalEnv *env)
{
    if (env != 0)
    {
        env->has_default_process = 0;
        env->default_pid = 0;
        env->default_pointer_width = Memmy_PointerWidth_Unknown;
    }
}

B32 Memmy_EvalEnv_GetDefaultProcess(Memmy_EvalEnv const *env, U32 *out_pid, Memmy_PointerWidth *out_pointer_width)
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

Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value)
{
    if (env == 0 || name.len == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        binding = Arena_PushStruct(env->arena, Memmy_EvalBinding);
        binding->name = String8_Copy(env->arena, name);
        binding->hash.hash = Hash_Fnv1a(binding->name.data, binding->name.len);
        HashMap_Insert(&env->bindings, &binding->hash);
    }
    binding->value = Memmy_EvalValue_Copy(env->arena, value);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_EvalEnv_Find(Arena *out_arena, Memmy_EvalEnv const *env, String8 name, Memmy_EvalValue *out)
{
    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }
    if (out_arena == 0 || env == 0 || out == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }

    *out = Memmy_EvalValue_Copy(out_arena, binding->value);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name)
{
    if (env == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }
    HashMap_Remove(&env->bindings, &binding->hash);
    return Memmy_Status_Ok;
}

void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env)
{
    if (env != 0)
    {
        env->bindings = HashMap_Create(env->arena);
    }
}
