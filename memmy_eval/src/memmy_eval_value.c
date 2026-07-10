#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

B32 Memmy_EvalValue_IsIntegerTyped(Memmy_EvalValue *value)
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

Memmy_Status Memmy_EvalValue_AsConst(Memmy_EvalValue *value, I64 *out, Memmy_Error *error)
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

Memmy_Status Memmy_EvalValue_AsAddress(Memmy_EvalValue *value, Memmy_Addr *out, Memmy_Error *error)
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

static void Memmy_EvalAddressList_Push(Arena *arena, List *list, Memmy_Addr address)
{
    Memmy_EvalAddressNode *node = Arena_PushStruct(arena, Memmy_EvalAddressNode);
    node->address = address;
    List_PushBack(list, &node->link);
}

static void Memmy_EvalRangeList_Push(Arena *arena, List *list, Memmy_Range range)
{
    Memmy_EvalRangeNode *node = Arena_PushStruct(arena, Memmy_EvalRangeNode);
    node->range = range;
    List_PushBack(list, &node->link);
}

static Memmy_EvalValue Memmy_Eval_AddressListFromList(Arena *arena, List *list)
{
    Memmy_Addr *addresses = 0;
    if (list->count != 0)
    {
        addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, list->count);
    }
    U64 index = 0;
    List_ForEach(Memmy_EvalAddressNode, node, list, link)
    {
        addresses[index++] = node->address;
    }
    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_AddressList,
        .addresses = addresses,
        .address_count = list->count,
    };
}

static Memmy_EvalValue Memmy_Eval_RangeListFromList(Arena *arena, List *list)
{
    Memmy_Range *ranges = 0;
    if (list->count != 0)
    {
        ranges = Arena_PushArrayNoZero(arena, Memmy_Range, list->count);
    }
    U64 index = 0;
    List_ForEach(Memmy_EvalRangeNode, node, list, link)
    {
        ranges[index++] = node->range;
    }
    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_RangeList,
        .ranges = ranges,
        .range_count = list->count,
    };
}

static Memmy_Status Memmy_EvalTransform_ListKindForValue(Memmy_EvalValue value, Memmy_EvalValueKind *out_kind,
                                                         Memmy_Error *error)
{
    if (value.kind == Memmy_EvalValueKind_Address || value.kind == Memmy_EvalValueKind_AddressList)
    {
        *out_kind = Memmy_EvalValueKind_AddressList;
        return Memmy_Status_Ok;
    }
    if (value.kind == Memmy_EvalValueKind_Range || value.kind == Memmy_EvalValueKind_RangeList)
    {
        *out_kind = Memmy_EvalValueKind_RangeList;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                    String8_Lit("transform expression must produce address or range values"));
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

Memmy_Status Memmy_Eval_AddressAddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
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

static B32 Memmy_EvalValue_IsAddressLike(Memmy_EvalValue value)
{
    return value.kind == Memmy_EvalValueKind_Address || value.kind == Memmy_EvalValueKind_Range;
}

static Memmy_Status Memmy_Eval_AddressDiff(Memmy_Addr lhs, Memmy_Addr rhs, I64 *out, Memmy_Error *error)
{
    if (lhs >= rhs)
    {
        U64 magnitude = lhs - rhs;
        if (magnitude > (U64)I64_MAX)
        {
            Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                            String8_Lit("address difference overflow"));
            return Memmy_Status_Overflow;
        }
        *out = (I64)magnitude;
        return Memmy_Status_Ok;
    }

    U64 magnitude = rhs - lhs;
    U64 limit = (U64)I64_MAX + 1ull;
    if (magnitude > limit)
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address difference overflow"));
        return Memmy_Status_Overflow;
    }
    *out = magnitude == limit ? I64_MIN : -(I64)magnitude;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_ApplyBinary(Memmy_AstConstOp op, Memmy_EvalValue lhs, Memmy_EvalValue rhs, Memmy_EvalValue *out,
                                    Memmy_Error *error)
{
    if (op == Memmy_AstConstOp_Add || op == Memmy_AstConstOp_Sub)
    {
        B32 lhs_address = Memmy_EvalValue_IsAddressLike(lhs);
        B32 rhs_address = Memmy_EvalValue_IsAddressLike(rhs);
        if (lhs_address && rhs_address)
        {
            if (op == Memmy_AstConstOp_Add)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                String8_Lit("cannot add two addresses"));
                return Memmy_Status_InvalidArgument;
            }

            Memmy_Addr lhs_addr = 0;
            Memmy_Addr rhs_addr = 0;
            Memmy_Status status = Memmy_EvalValue_AsAddress(&lhs, &lhs_addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_EvalValue_AsAddress(&rhs, &rhs_addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            I64 diff = 0;
            status = Memmy_Eval_AddressDiff(lhs_addr, rhs_addr, &diff, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = diff};
            return Memmy_Status_Ok;
        }

        if (lhs_address)
        {
            I64 constant = 0;
            Memmy_Status status = Memmy_EvalValue_AsConst(&rhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            Memmy_Addr address = 0;
            status = Memmy_EvalValue_AsAddress(&lhs, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = op == Memmy_AstConstOp_Add ? Memmy_Eval_AddressAddConst(address, constant, &address, error)
                                                : Memmy_Eval_AddressSubConst(address, constant, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = address,
            };
            return Memmy_Status_Ok;
        }

        if (rhs_address)
        {
            if (op == Memmy_AstConstOp_Sub)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                String8_Lit("cannot subtract an address from a constant"));
                return Memmy_Status_InvalidArgument;
            }

            I64 constant = 0;
            Memmy_Status status = Memmy_EvalValue_AsConst(&lhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            Memmy_Addr address = 0;
            status = Memmy_EvalValue_AsAddress(&rhs, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Eval_AddressAddConst(address, constant, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = address,
            };
            return Memmy_Status_Ok;
        }
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

static Memmy_Status Memmy_EvalTransform_Append(Memmy_EvalExec *exec, Memmy_EvalValue value, List *addresses,
                                               List *ranges, Memmy_EvalValueKind *out_kind, Memmy_Error *error)
{
    Memmy_EvalValueKind value_kind = Memmy_EvalValueKind_Null;
    Memmy_Status status = Memmy_EvalTransform_ListKindForValue(value, &value_kind, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (*out_kind == Memmy_EvalValueKind_Null)
    {
        *out_kind = value_kind;
    }
    else if (*out_kind != value_kind)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                        String8_Lit("transform expression produced mixed address and range values"));
        return Memmy_Status_InvalidArgument;
    }

    Arena *arena = exec->env->arena;
    if (value.kind == Memmy_EvalValueKind_Address)
    {
        Memmy_EvalAddressList_Push(arena, addresses, value.address);
    }
    else if (value.kind == Memmy_EvalValueKind_AddressList)
    {
        for (U64 i = 0; i < value.address_count; i++)
        {
            Memmy_EvalAddressList_Push(arena, addresses, value.addresses[i]);
        }
    }
    else if (value.kind == Memmy_EvalValueKind_Range)
    {
        Memmy_EvalRangeList_Push(arena, ranges, value.range);
    }
    else if (value.kind == Memmy_EvalValueKind_RangeList)
    {
        for (U64 i = 0; i < value.range_count; i++)
        {
            Memmy_EvalRangeList_Push(arena, ranges, value.ranges[i]);
        }
    }

    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_ListTransform(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out,
                                      Memmy_Error *error)
{
    Memmy_EvalValue list = {0};
    Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &list, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (list.kind != Memmy_EvalValueKind_AddressList && list.kind != Memmy_EvalValueKind_RangeList)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                        String8_Lit("expected address list or range list"));
        return Memmy_Status_InvalidArgument;
    }

    U64 count = list.kind == Memmy_EvalValueKind_AddressList ? list.address_count : list.range_count;
    if (count == 0)
    {
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("transform"),
                        String8_Lit("transform input list is empty"));
        return Memmy_Status_NotFound;
    }

    List addresses = {0}; // Memmy_EvalAddressNode
    List ranges = {0};    // Memmy_EvalRangeNode
    Memmy_EvalValueKind out_kind = Memmy_EvalValueKind_Null;
    B32 old_has_current_item = exec->has_current_item;
    Memmy_EvalValue old_current_item = exec->current_item;

    for (U64 i = 0; i < count; i++)
    {
        exec->has_current_item = 1;
        if (list.kind == Memmy_EvalValueKind_AddressList)
        {
            exec->current_item = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = list.addresses[i]};
        }
        else
        {
            exec->current_item = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = list.ranges[i]};
        }

        Memmy_EvalValue item_result = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &item_result, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
        status = Memmy_EvalTransform_Append(exec, item_result, &addresses, &ranges, &out_kind, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
    }

    exec->has_current_item = old_has_current_item;
    exec->current_item = old_current_item;
    if (out_kind == Memmy_EvalValueKind_RangeList)
    {
        *out = Memmy_Eval_RangeListFromList(exec->env->arena, &ranges);
    }
    else
    {
        *out = Memmy_Eval_AddressListFromList(exec->env->arena, &addresses);
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_ValueExpr(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)env;
    if (expr->kind == Memmy_AstNodeKind_ConstArithmetic)
    {
        if (expr->op == Memmy_AstConstOp_None)
        {
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = expr->value};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue lhs = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &lhs, error);
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
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &rhs, error);
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
    if (expr->kind == Memmy_AstNodeKind_CurrentItem)
    {
        if (!exec->has_current_item)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                            String8_Lit("current item is only available inside transforms"));
            return Memmy_Status_InvalidArgument;
        }
        *out = exec->current_item;
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ListTransform)
    {
        return Memmy_Eval_ListTransform(exec, expr, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_Address)
    {
        if (expr->value_expr != 0)
        {
            Memmy_EvalValue value = {0};
            Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->value_expr, &value, error);
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
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = (Memmy_Addr)constant,
            };
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
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
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &offset_value, error);
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
        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_Address,
            .address = address,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Range)
    {
        Memmy_EvalValue start_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &start_value, error);
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
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &rhs, error);
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

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_Range,
            .range = range,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Index)
    {
        Memmy_EvalValue list = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &list, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (list.kind != Memmy_EvalValueKind_AddressList && list.kind != Memmy_EvalValueKind_RangeList)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("index"),
                            String8_Lit("expected address list or range list"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_EvalValue index_value = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &index_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 index = 0;
        status = Memmy_EvalValue_AsConst(&index_value, &index, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        U64 count = list.kind == Memmy_EvalValueKind_AddressList ? list.address_count : list.range_count;
        if (index < 0 || (U64)index >= count)
        {
            Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("index"), String8_Lit("list index out of range"));
            return Memmy_Status_NotFound;
        }

        if (list.kind == Memmy_EvalValueKind_AddressList)
        {
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = list.addresses[index],
            };
        }
        else
        {
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Range,
                .range = list.ranges[index],
            };
        }
        return Memmy_Status_Ok;
    }
    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
