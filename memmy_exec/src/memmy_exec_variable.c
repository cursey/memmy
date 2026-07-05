#include "memmy_exec.h"

#include "base_checked.h"
#include "base_hash.h"

struct Memmy_ExecVariableBinding
{
    HashLink hash;
    String8 name;
    Memmy_VariableExpr expr;
};

struct Memmy_ExecResolveFrame
{
    Memmy_ExecResolveFrame *next;
    String8 name;
};

static B32 Memmy_ExecVariableBinding_NameEq(void *link, void *ctx)
{
    Memmy_ExecVariableBinding *binding = ContainerOf((HashLink *)link, Memmy_ExecVariableBinding, hash);
    String8 *name = (String8 *)ctx;
    return String8_Eq(binding->name, *name);
}

static U64 Memmy_ExecVariableName_Hash(String8 name)
{
    return Hash_Fnv1a(name.data, name.len);
}

static Memmy_ConstExpr *Memmy_Exec_CloneConstExprPtr(Arena *arena, Memmy_ConstExpr *expr);

static Memmy_VariableRef Memmy_Exec_CloneVariableRef(Arena *arena, Memmy_VariableRef variable)
{
    return (Memmy_VariableRef){
        .name = String8_Copy(arena, variable.name),
    };
}

static Memmy_ProcessSelector Memmy_Exec_CloneProcessSelector(Arena *arena, Memmy_ProcessSelector selector)
{
    if (selector.kind == Memmy_ProcessSelectorKind_Name)
    {
        selector.name = String8_Copy(arena, selector.name);
    }
    return selector;
}

static Memmy_TargetExpr Memmy_Exec_CloneTargetExpr(Arena *arena, Memmy_TargetExpr target)
{
    target.process = Memmy_Exec_CloneProcessSelector(arena, target.process);
    target.module_name = String8_Copy(arena, target.module_name);
    return target;
}

static Memmy_ConstExpr Memmy_Exec_CloneConstExpr(Arena *arena, Memmy_ConstExpr expr)
{
    expr.variable = Memmy_Exec_CloneVariableRef(arena, expr.variable);
    expr.lhs = Memmy_Exec_CloneConstExprPtr(arena, expr.lhs);
    expr.rhs = Memmy_Exec_CloneConstExprPtr(arena, expr.rhs);
    return expr;
}

static Memmy_ConstExpr *Memmy_Exec_CloneConstExprPtr(Arena *arena, Memmy_ConstExpr *expr)
{
    if (expr == 0)
    {
        return 0;
    }

    Memmy_ConstExpr *copy = Arena_PushStruct(arena, Memmy_ConstExpr);
    *copy = Memmy_Exec_CloneConstExpr(arena, *expr);
    return copy;
}

static Memmy_AddressExpr Memmy_Exec_CloneAddressExpr(Arena *arena, Memmy_AddressExpr expr)
{
    Memmy_AddressExpr copy = expr;
    copy.target = Memmy_Exec_CloneTargetExpr(arena, expr.target);
    copy.variable = Memmy_Exec_CloneVariableRef(arena, expr.variable);
    copy.ops = (List){0};
    List_ForEach(Memmy_AddressOp, op, &expr.ops, link)
    {
        Memmy_AddressOp *op_copy = Arena_PushStruct(arena, Memmy_AddressOp);
        op_copy->kind = op->kind;
        op_copy->offset = op->offset;
        op_copy->offset_expr = Memmy_Exec_CloneConstExpr(arena, op->offset_expr);
        List_PushBack(&copy.ops, &op_copy->link);
    }
    return copy;
}

static Memmy_RangeExpr Memmy_Exec_CloneRangeExpr(Arena *arena, Memmy_RangeExpr expr)
{
    expr.target = Memmy_Exec_CloneTargetExpr(arena, expr.target);
    expr.variable = Memmy_Exec_CloneVariableRef(arena, expr.variable);
    expr.start_offset_expr = Memmy_Exec_CloneConstExpr(arena, expr.start_offset_expr);
    expr.end_offset_expr = Memmy_Exec_CloneConstExpr(arena, expr.end_offset_expr);
    expr.size_expr = Memmy_Exec_CloneConstExpr(arena, expr.size_expr);
    expr.address = Memmy_Exec_CloneAddressExpr(arena, expr.address);
    return expr;
}

static Memmy_VariableExpr Memmy_Exec_CloneVariableExpr(Arena *arena, Memmy_VariableExpr expr)
{
    expr.address = Memmy_Exec_CloneAddressExpr(arena, expr.address);
    expr.range = Memmy_Exec_CloneRangeExpr(arena, expr.range);
    expr.constant = Memmy_Exec_CloneConstExpr(arena, expr.constant);
    return expr;
}

Memmy_ExecEnv Memmy_ExecEnv_Create(Arena *arena)
{
    Memmy_ExecEnv env = {
        .arena = arena,
        .bindings = HashMap_Create(arena),
    };
    return env;
}

Memmy_Status Memmy_ExecEnv_Find(Memmy_ExecEnv *env, String8 name, Memmy_ExecVariableBinding **out, Memmy_Error *error)
{
    if (env == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("variable"),
                        String8_Lit("missing variable environment or output binding"));
        return Memmy_Status_InvalidArgument;
    }

    U64 hash = Memmy_ExecVariableName_Hash(name);
    HashLink *link = HashMap_Find(&env->bindings, hash, Memmy_ExecVariableBinding_NameEq, &name);
    if (link == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("variable"),
                        String8_PushF(env->arena, "variable not found: $%.*s", (int)name.len, (char *)name.data));
        return Memmy_Status_NotFound;
    }

    *out = ContainerOf(link, Memmy_ExecVariableBinding, hash);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_ExecEnv_Set(Memmy_ExecEnv *env, String8 name, Memmy_VariableExpr *expr, Memmy_Error *error)
{
    if (env == 0 || env->arena == 0 || expr == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("variable"),
                        String8_Lit("missing variable environment or expression"));
        return Memmy_Status_InvalidArgument;
    }

    U64 hash = Memmy_ExecVariableName_Hash(name);
    HashLink *link = HashMap_Find(&env->bindings, hash, Memmy_ExecVariableBinding_NameEq, &name);
    Memmy_ExecVariableBinding *binding = 0;
    if (link != 0)
    {
        binding = ContainerOf(link, Memmy_ExecVariableBinding, hash);
    }
    else
    {
        binding = Arena_PushStruct(env->arena, Memmy_ExecVariableBinding);
        binding->hash.hash = hash;
        binding->name = String8_Copy(env->arena, name);
        HashMap_Insert(&env->bindings, &binding->hash);
    }

    binding->expr = Memmy_Exec_CloneVariableExpr(env->arena, *expr);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_ExecEnv_Unset(Memmy_ExecEnv *env, String8 name, Memmy_Error *error)
{
    Memmy_ExecVariableBinding *binding = 0;
    Memmy_Status status = Memmy_ExecEnv_Find(env, name, &binding, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    HashMap_Remove(&env->bindings, &binding->hash);
    return Memmy_Status_Ok;
}

Memmy_ExecVariableBinding *Memmy_ExecEnv_First(Memmy_ExecEnv *env)
{
    if (env == 0)
    {
        return 0;
    }
    HashLink *link = HashMap_First(&env->bindings);
    return link != 0 ? ContainerOf(link, Memmy_ExecVariableBinding, hash) : 0;
}

Memmy_ExecVariableBinding *Memmy_ExecEnv_Next(Memmy_ExecEnv *env, Memmy_ExecVariableBinding *binding)
{
    if (env == 0 || binding == 0)
    {
        return 0;
    }
    HashLink *link = HashMap_Next(&env->bindings, &binding->hash);
    return link != 0 ? ContainerOf(link, Memmy_ExecVariableBinding, hash) : 0;
}

String8 Memmy_ExecVariableBinding_Name(Memmy_ExecVariableBinding *binding)
{
    return binding != 0 ? binding->name : (String8){0};
}

Memmy_VariableExprKind Memmy_ExecVariableBinding_Kind(Memmy_ExecVariableBinding *binding)
{
    return binding != 0 ? binding->expr.kind : 0;
}

Memmy_VariableExpr *Memmy_ExecVariableBinding_Expr(Memmy_ExecVariableBinding *binding)
{
    return binding != 0 ? &binding->expr : 0;
}

static String8 Memmy_ExecEnv_CycleMessage(Memmy_ExecEnv *env, String8 name)
{
    Scratch scratch = Scratch_Begin(&env->arena, 1);
    String8List parts = {0};
    String8List_Push(scratch.arena, &parts, String8_Lit("variable cycle: "));

    U64 frame_count = 0;
    for (Memmy_ExecResolveFrame *frame = env->resolve_stack; frame != 0; frame = frame->next)
    {
        frame_count++;
    }
    Memmy_ExecResolveFrame **frames = Arena_PushArrayNoZero(scratch.arena, Memmy_ExecResolveFrame *, frame_count);
    U64 index = frame_count;
    for (Memmy_ExecResolveFrame *frame = env->resolve_stack; frame != 0; frame = frame->next)
    {
        frames[--index] = frame;
    }

    B32 in_cycle = 0;
    U64 count = 0;
    for (U64 i = 0; i < frame_count; i++)
    {
        if (!in_cycle && String8_Eq(frames[i]->name, name))
        {
            in_cycle = 1;
        }
        if (in_cycle)
        {
            if (count > 0)
            {
                String8List_Push(scratch.arena, &parts, String8_Lit(" -> "));
            }
            String8List_Push(
                scratch.arena, &parts,
                String8_PushF(scratch.arena, "$%.*s", (int)frames[i]->name.len, (char *)frames[i]->name.data));
            count++;
        }
    }
    String8List_Push(scratch.arena, &parts, String8_Lit(" -> "));
    String8List_Push(scratch.arena, &parts, String8_PushF(scratch.arena, "$%.*s", (int)name.len, (char *)name.data));

    String8 result = String8_Copy(env->arena, String8List_Join(scratch.arena, &parts, (String8){0}));
    Scratch_End(scratch);
    return result;
}

Memmy_Status Memmy_ExecEnv_ResolvePush(Memmy_ExecEnv *env, String8 name, Memmy_Error *error)
{
    if (env == 0 || env->arena == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("variable"),
                        String8_Lit("missing variable environment"));
        return Memmy_Status_InvalidArgument;
    }

    for (Memmy_ExecResolveFrame *frame = env->resolve_stack; frame != 0; frame = frame->next)
    {
        if (String8_Eq(frame->name, name))
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("variable"),
                            Memmy_ExecEnv_CycleMessage(env, name));
            return Memmy_Status_InvalidArgument;
        }
    }

    Memmy_ExecResolveFrame *frame = Arena_PushStruct(env->arena, Memmy_ExecResolveFrame);
    frame->name = name;
    frame->next = env->resolve_stack;
    env->resolve_stack = frame;
    return Memmy_Status_Ok;
}

void Memmy_ExecEnv_ResolvePop(Memmy_ExecEnv *env)
{
    if (env != 0 && env->resolve_stack != 0)
    {
        env->resolve_stack = env->resolve_stack->next;
    }
}

static Memmy_Status Memmy_ConstExpr_ApplyBinary(U8 op, I64 lhs, I64 rhs, I64 *out, Memmy_Error *error)
{
    B32 ok = 0;
    if (op == '+')
    {
        ok = AddI64Checked(lhs, rhs, out);
    }
    else if (op == '-')
    {
        ok = SubI64Checked(lhs, rhs, out);
    }
    else if (op == '*')
    {
        ok = MulI64Checked(lhs, rhs, out);
    }
    else if (op == '/')
    {
        if (rhs == 0)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("expr"), String8_Lit("division by zero"));
            return Memmy_Status_ParseError;
        }
        ok = DivI64Checked(lhs, rhs, out);
    }
    else if (op == '%')
    {
        if (rhs == 0)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("expr"), String8_Lit("modulo by zero"));
            return Memmy_Status_ParseError;
        }
        ok = ModI64Checked(lhs, rhs, out);
    }

    if (!ok)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant expression overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_ConstExpr_Resolve(Memmy_ExecEnv *env, Memmy_Process *process, Memmy_ConstExpr *expr, I64 *out,
                                     Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing constant expression or output value"));
        return Memmy_Status_InvalidArgument;
    }

    if (expr->kind == Memmy_ConstExprKind_Literal)
    {
        *out = expr->value;
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_ConstExprKind_Variable)
    {
        Memmy_Status status = Memmy_ExecEnv_ResolvePush(env, expr->variable.name, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_ExecVariableBinding *binding = 0;
        status = Memmy_ExecEnv_Find(env, expr->variable.name, &binding, error);
        if (status == Memmy_Status_Ok && binding->expr.kind != Memmy_VariableExprKind_Const)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("variable"),
                            String8_PushF(env->arena, "wrong variable kind for $%.*s: expected const",
                                          (int)expr->variable.name.len, (char *)expr->variable.name.data));
            status = Memmy_Status_InvalidArgument;
        }
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_ConstExpr_Resolve(env, process, &binding->expr.constant, out, error);
        }
        Memmy_ExecEnv_ResolvePop(env);
        return status;
    }
    if (expr->kind == Memmy_ConstExprKind_Unary)
    {
        I64 value = 0;
        Memmy_Status status = Memmy_ConstExpr_Resolve(env, process, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == '+')
        {
            *out = value;
            return Memmy_Status_Ok;
        }
        if (expr->op == '-' && !SubI64Checked(0, value, out))
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                            String8_Lit("constant expression overflow"));
            return Memmy_Status_Overflow;
        }
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_ConstExprKind_Binary)
    {
        I64 lhs = 0;
        I64 rhs = 0;
        Memmy_Status status = Memmy_ConstExpr_Resolve(env, process, expr->lhs, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_ConstExpr_Resolve(env, process, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_ConstExpr_ApplyBinary(expr->op, lhs, rhs, out, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                    String8_Lit("unknown constant expression kind"));
    return Memmy_Status_InvalidArgument;
}
