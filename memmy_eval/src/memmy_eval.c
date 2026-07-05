#include "memmy_eval.h"

#include "base_checked.h"
#include "base_hash.h"

struct Memmy_EvalBinding
{
    HashLink hash;
    String8 name;
    Memmy_EvalValue value;
};

typedef struct Memmy_EvalProcessResolver Memmy_EvalProcessResolver;
struct Memmy_EvalProcessResolver
{
    Memmy_AstNode *target;
    Memmy_ProcessInfo match;
    U64 match_count;
    Memmy_Error *error;
};

typedef struct Memmy_EvalModuleResolver Memmy_EvalModuleResolver;
struct Memmy_EvalModuleResolver
{
    String8 name;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};

typedef struct Memmy_EvalRegionResolver Memmy_EvalRegionResolver;
struct Memmy_EvalRegionResolver
{
    Memmy_Range range;
    B32 any;
};

static void Memmy_EvalError(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    Memmy_Error_Set(error, status, context, message);
}

static B32 Memmy_EvalBinding_Eq(void *link, void *ctx)
{
    Memmy_EvalBinding *binding = ContainerOf((HashLink *)link, Memmy_EvalBinding, hash);
    String8 *name = (String8 *)ctx;
    return String8_Eq(binding->name, *name);
}

static Memmy_EvalBinding *Memmy_EvalEnv_FindBinding(Memmy_EvalEnv *env, String8 name)
{
    U64 hash = Hash_Fnv1a(name.data, name.len);
    HashLink *link = HashMap_Find(&env->bindings, hash, Memmy_EvalBinding_Eq, &name);
    return link != 0 ? ContainerOf(link, Memmy_EvalBinding, hash) : 0;
}

static Memmy_EvalValue Memmy_EvalValue_Copy(Arena *arena, Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        value.typed_value.bytes = String8_Copy(arena, value.typed_value.bytes);
    }
    return value;
}

static B32 Memmy_EvalValue_IsIntegerTyped(Memmy_EvalValue *value)
{
    if (value->kind != Memmy_EvalValueKind_TypedValue)
    {
        return 0;
    }

    Memmy_TypeKind kind = value->typed_value.type.kind;
    return kind == Memmy_TypeKind_U8 || kind == Memmy_TypeKind_I8 || kind == Memmy_TypeKind_U16 ||
           kind == Memmy_TypeKind_I16 || kind == Memmy_TypeKind_U32 || kind == Memmy_TypeKind_I32 ||
           kind == Memmy_TypeKind_U64 || kind == Memmy_TypeKind_I64 || kind == Memmy_TypeKind_Ptr;
}

static Memmy_Status Memmy_EvalValue_AsConst(Memmy_EvalValue *value, I64 *out, Memmy_Error *error)
{
    if (value->kind == Memmy_EvalValueKind_Const)
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }
    if (Memmy_EvalValue_IsIntegerTyped(value))
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                    String8_Lit("expected constant integer value"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_EvalValue_AsAddress(Memmy_EvalValue *value, Memmy_Addr *out, Memmy_Error *error)
{
    if (value->kind == Memmy_EvalValueKind_Address)
    {
        *out = value->address;
        return Memmy_Status_Ok;
    }
    if (value->kind == Memmy_EvalValueKind_Range)
    {
        *out = value->range.start;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"), String8_Lit("expected address value"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Eval_AddConst(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!AddI64Checked(a, b, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_SubConst(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!SubI64Checked(a, b, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_AddressAddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
{
    if (!AddI64ToU64Checked(address, constant, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_AddressSubConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
{
    B32 ok = 0;
    if (constant >= 0)
    {
        ok = SubU64Checked(address, (U64)constant, out);
    }
    else
    {
        U64 magnitude = (U64)(-(constant + 1)) + 1;
        ok = AddU64Checked(address, magnitude, out);
    }

    if (!ok)
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ApplyBinary(Memmy_AstConstOp op, Memmy_EvalValue lhs, Memmy_EvalValue rhs,
                                           Memmy_EvalValue *out, Memmy_Error *error)
{
    if ((op == Memmy_AstConstOp_Add || op == Memmy_AstConstOp_Sub) && lhs.kind == Memmy_EvalValueKind_Address)
    {
        I64 constant = 0;
        Memmy_Status status = Memmy_EvalValue_AsConst(&rhs, &constant, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Addr address = 0;
        status = op == Memmy_AstConstOp_Add ? Memmy_Eval_AddressAddConst(lhs.address, constant, &address, error)
                                            : Memmy_Eval_AddressSubConst(lhs.address, constant, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = address};
        return Memmy_Status_Ok;
    }

    I64 a = 0;
    I64 b = 0;
    Memmy_Status status = Memmy_EvalValue_AsConst(&lhs, &a, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_EvalValue_AsConst(&rhs, &b, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    I64 result = 0;
    switch (op)
    {
    case Memmy_AstConstOp_Add:
        status = Memmy_Eval_AddConst(a, b, &result, error);
        break;
    case Memmy_AstConstOp_Sub:
        status = Memmy_Eval_SubConst(a, b, &result, error);
        break;
    case Memmy_AstConstOp_Mul:
        if (!MulI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case Memmy_AstConstOp_Div:
        if (b == 0)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"), String8_Lit("division by zero"));
            return Memmy_Status_InvalidArgument;
        }
        if (!DivI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case Memmy_AstConstOp_Mod:
        if (b == 0)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"), String8_Lit("modulo by zero"));
            return Memmy_Status_InvalidArgument;
        }
        if (!ModI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    default:
        status = Memmy_Status_InvalidArgument;
        break;
    }

    if (status == Memmy_Status_Overflow)
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return status;
    }
    if (status != Memmy_Status_Ok)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("unsupported arithmetic expression"));
        return status;
    }

    *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = result};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ResolveProcessByPid(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    return Memmy_Process_Open(arena, pid, out, error);
}

static Memmy_Status Memmy_EvalProcessResolver_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_EvalProcessResolver *resolver = (Memmy_EvalProcessResolver *)user_data;
    if (!String8_EqNoCase(info->name, resolver->target->target_process))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        Memmy_EvalError(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                        String8_Lit("process target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *info;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ResolveProcessByName(Arena *arena, Memmy_AstNode *target, Memmy_Process **out,
                                                    Memmy_Error *error)
{
    Memmy_EvalProcessResolver resolver = {
        .target = target,
        .error = error,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = Memmy_EvalProcessResolver_Push,
        .user_data = &resolver,
    };
    Memmy_Status status = Memmy_EnumerateProcesses(arena, sink, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (resolver.match_count == 0)
    {
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("process target not found"));
        return Memmy_Status_NotFound;
    }

    return Memmy_Process_Open(arena, resolver.match.pid, out, error);
}

static Memmy_Status Memmy_Eval_ResolveTargetProcess(Memmy_EvalEnv *env, Memmy_AstNode *target, Memmy_Process **out,
                                                    Memmy_Error *error)
{
    if (!target->target_has_process)
    {
        if (env->process == 0 || !Memmy_Process_IsOpen(env->process))
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                            String8_Lit("missing selected process for target"));
            return Memmy_Status_InvalidArgument;
        }
        *out = env->process;
        return Memmy_Status_Ok;
    }

    if (target->target_process_is_pid)
    {
        Memmy_Size pid64 = 0;
        Memmy_Status status = Memmy_ParseSize(target->target_process, &pid64, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (pid64 > U32_MAX)
        {
            Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("target"), String8_Lit("pid overflow"));
            return Memmy_Status_Overflow;
        }
        if (env->process != 0 && Memmy_Process_IsOpen(env->process) && env->process->pid == (U32)pid64)
        {
            *out = env->process;
            return Memmy_Status_Ok;
        }
        return Memmy_Eval_ResolveProcessByPid(env->arena, (U32)pid64, out, error);
    }

    if (env->process != 0 && Memmy_Process_IsOpen(env->process))
    {
        Scratch scratch = Scratch_Begin(&env->arena, 1);
        Memmy_EvalProcessResolver resolver = {
            .target = target,
            .error = error,
        };
        Memmy_ProcessInfoSink sink = {
            .callback = Memmy_EvalProcessResolver_Push,
            .user_data = &resolver,
        };
        Memmy_Status status = Memmy_EnumerateProcesses(scratch.arena, sink, error);
        Scratch_End(scratch);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (resolver.match_count == 1 && resolver.match.pid == env->process->pid)
        {
            *out = env->process;
            return Memmy_Status_Ok;
        }
    }

    return Memmy_Eval_ResolveProcessByName(env->arena, target, out, error);
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

static Memmy_Status Memmy_Eval_ResolveModule(Memmy_EvalEnv *env, Memmy_AstNode *target, Memmy_Module *out,
                                             Memmy_Process **out_process, Memmy_Error *error)
{
    Memmy_Process *process = 0;
    Memmy_Status status = Memmy_Eval_ResolveTargetProcess(env, target, &process, error);
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

static Memmy_Status Memmy_EvalRegionResolver_Push(void *user_data, Memmy_Region *region)
{
    Memmy_EvalRegionResolver *resolver = (Memmy_EvalRegionResolver *)user_data;
    if (region->state != Memmy_RegionState_Committed)
    {
        return Memmy_Status_Ok;
    }

    Memmy_Addr end = 0;
    if (!AddU64Checked(region->base, region->size, &end))
    {
        return Memmy_Status_Overflow;
    }

    if (!resolver->any)
    {
        resolver->range = (Memmy_Range){.start = region->base, .end = end};
        resolver->any = 1;
    }
    else
    {
        resolver->range.start = Min(resolver->range.start, region->base);
        resolver->range.end = Max(resolver->range.end, end);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_Target(Memmy_EvalEnv *env, Memmy_AstNode *target, Memmy_EvalValue *out,
                                      Memmy_Error *error)
{
    if (target->target_module.len != 0)
    {
        Memmy_Module module = {0};
        Memmy_Status status = Memmy_Eval_ResolveModule(env, target, &module, 0, error);
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

    Memmy_Process *process = 0;
    Memmy_Status status = Memmy_Eval_ResolveTargetProcess(env, target, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(&env->arena, 1);
    Memmy_EvalRegionResolver resolver = {0};
    Memmy_RegionSink sink = {
        .callback = Memmy_EvalRegionResolver_Push,
        .user_data = &resolver,
    };
    status = Memmy_Process_EnumerateRegions(scratch.arena, process, sink, error);
    Scratch_End(scratch);
    if (status != Memmy_Status_Ok)
    {
        if (status == Memmy_Status_Overflow)
        {
            Memmy_EvalError(error, status, String8_Lit("target"), String8_Lit("process range overflow"));
        }
        return status;
    }
    if (!resolver.any)
    {
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("target"),
                        String8_Lit("process target has no regions"));
        return Memmy_Status_NotFound;
    }

    *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = resolver.range};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ReadPointer(Memmy_EvalEnv *env, Memmy_Addr address, Memmy_Addr *out, Memmy_Error *error)
{
    if (env->process == 0 || !Memmy_Process_IsOpen(env->process))
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("missing selected process for pointer dereference"));
        return Memmy_Status_InvalidArgument;
    }

    U64 pointer_size = 0;
    if (env->process->pointer_width == Memmy_PointerWidth_32)
    {
        pointer_size = 4;
    }
    else if (env->process->pointer_width == Memmy_PointerWidth_64)
    {
        pointer_size = 8;
    }
    else
    {
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("address"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }

    U8 bytes[8] = {0};
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Process_Read(env->process, address, bytes, pointer_size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != pointer_size)
    {
        Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("address"),
                        String8_Lit("pointer read returned too few bytes"));
        return Memmy_Status_PartialRead;
    }

    Memmy_Addr value = 0;
    for (U64 i = 0; i < pointer_size; i++)
    {
        value |= ((Memmy_Addr)bytes[i]) << (i * 8);
    }
    *out = value;
    return Memmy_Status_Ok;
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

Memmy_Status Memmy_EvalStatement(Memmy_EvalEnv *env, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                 Memmy_Error *error)
{
    if (env == 0 || statement == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                        String8_Lit("missing eval environment or statement"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalValue value = {0};
    Memmy_Status status = Memmy_Status_Ok;
    if (statement->kind == Memmy_AstNodeKind_Assignment)
    {
        status = Memmy_EvalExpr(env, statement->assignment_value, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_EvalEnv_Set(env, statement->assignment_name, value);
    }
    else if (statement->kind == Memmy_AstNodeKind_Command)
    {
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("statement"),
                        String8_Lit("commands are not implemented yet"));
        return Memmy_Status_Unsupported;
    }
    else if (statement->expr != 0)
    {
        status = Memmy_EvalExpr(env, statement->expr, &value, error);
    }
    else
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                        String8_Lit("missing statement expression"));
        return Memmy_Status_InvalidArgument;
    }

    if (status == Memmy_Status_Ok && sink != 0 && sink->push != 0)
    {
        sink->push(sink, (Memmy_EvalResult){.value = value});
    }
    return status;
}

Memmy_Status Memmy_EvalExpr(Memmy_EvalEnv *env, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }
    if (env == 0 || expr == 0 || out == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                        String8_Lit("missing eval environment, expression, or output"));
        return Memmy_Status_InvalidArgument;
    }

    if (expr->kind == Memmy_AstNodeKind_ConstArithmetic)
    {
        if (expr->op == Memmy_AstConstOp_None)
        {
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = expr->value};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue lhs = {0};
        Memmy_Status status = Memmy_EvalExpr(env, expr->lhs, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == Memmy_AstConstOp_Pos || expr->op == Memmy_AstConstOp_Neg)
        {
            I64 constant = 0;
            status = Memmy_EvalValue_AsConst(&lhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (expr->op == Memmy_AstConstOp_Neg && !SubI64Checked(0, constant, &constant))
            {
                Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"),
                                String8_Lit("constant arithmetic overflow"));
                return Memmy_Status_Overflow;
            }
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = constant};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue rhs = {0};
        status = Memmy_EvalExpr(env, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Eval_ApplyBinary(expr->op, lhs, rhs, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_Variable)
    {
        return Memmy_EvalEnv_Find(env, expr->name, out);
    }
    if (expr->kind == Memmy_AstNodeKind_Target)
    {
        return Memmy_Eval_Target(env, expr, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_Address)
    {
        if (expr->value_expr != 0)
        {
            Memmy_EvalValue value = {0};
            Memmy_Status status = Memmy_EvalExpr(env, expr->value_expr, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            I64 constant = 0;
            status = Memmy_EvalValue_AsConst(&value, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (constant < 0)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                                String8_Lit("address cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = (Memmy_Addr)constant};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExpr(env, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalValue offset_value = {0};
        status = Memmy_EvalExpr(env, expr->rhs, &offset_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 offset = 0;
        status = Memmy_EvalValue_AsConst(&offset_value, &offset, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Eval_AddressAddConst(address, offset, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = address};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Range)
    {
        Memmy_EvalValue start_value = {0};
        Memmy_Status status = Memmy_EvalExpr(env, expr->lhs, &start_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr start = 0;
        status = Memmy_EvalValue_AsAddress(&start_value, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalValue rhs = {0};
        status = Memmy_EvalExpr(env, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        if (expr->range_is_sized)
        {
            I64 size = 0;
            status = Memmy_EvalValue_AsConst(&rhs, &size, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (size < 0)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                                String8_Lit("range size cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            status = Memmy_Range_FromStartLength(start, (Memmy_Size)size, &range, error);
        }
        else
        {
            Memmy_Addr end = 0;
            status = Memmy_EvalValue_AsAddress(&rhs, &end, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Range_FromStartEnd(start, end, &range, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = range};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Deref)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExpr(env, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Eval_ReadPointer(env, address, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->rhs != 0)
        {
            Memmy_EvalValue offset_value = {0};
            status = Memmy_EvalExpr(env, expr->rhs, &offset_value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            I64 offset = 0;
            status = Memmy_EvalValue_AsConst(&offset_value, &offset, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Eval_AddressAddConst(address, offset, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = address};
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
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

Memmy_Status Memmy_EvalEnv_Find(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue *out)
{
    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }
    if (env == 0 || out == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }

    *out = binding->value;
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
