#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

B32 MemmyEval_Value_IsIntegerTyped(MemmyEval_Value *value)
{
    if (value->kind != MemmyEval_ValueKind_TypedValue)
    {
        return 0;
    }

    Memmy_TypeKind kind = value->typed_value.type.kind;
    return kind == Memmy_TypeKind_U8 || kind == Memmy_TypeKind_I8 || kind == Memmy_TypeKind_U16 ||
           kind == Memmy_TypeKind_I16 || kind == Memmy_TypeKind_U32 || kind == Memmy_TypeKind_I32 ||
           kind == Memmy_TypeKind_U64 || kind == Memmy_TypeKind_I64 || kind == Memmy_TypeKind_Ptr;
}

Memmy_Status MemmyEval_Value_AsConst(MemmyEval_Value *value, I64 *out, Memmy_Error *error)
{
    if (value->kind == MemmyEval_ValueKind_Const)
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }
    if (MemmyEval_Value_IsIntegerTyped(value))
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }

    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("expected constant integer value"));
    return Memmy_Status_InvalidArgument;
}

Memmy_Status MemmyEval_Value_AsAddress(MemmyEval_Value *value, Memmy_Addr *out, Memmy_Error *error)
{
    if (value->kind == MemmyEval_ValueKind_Address)
    {
        *out = value->address;
        return Memmy_Status_Ok;
    }
    if (value->kind == MemmyEval_ValueKind_Range)
    {
        *out = value->range.start;
        return Memmy_Status_Ok;
    }

    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("expected address value"));
    return Memmy_Status_InvalidArgument;
}

static void MemmyEval_AddressList_Push(Arena *arena, List *list, Memmy_Addr address)
{
    MemmyEval_AddressNode *node = Arena_PushStruct(arena, MemmyEval_AddressNode);
    node->address = address;
    List_PushBack(list, &node->link);
}

static void MemmyEval_RangeList_Push(Arena *arena, List *list, Memmy_Range range)
{
    MemmyEval_RangeNode *node = Arena_PushStruct(arena, MemmyEval_RangeNode);
    node->range = range;
    List_PushBack(list, &node->link);
}

static MemmyEval_Value MemmyEval_AddressList_FromList(Arena *arena, List *list)
{
    Memmy_Addr *addresses = 0;
    if (list->count != 0)
    {
        addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, list->count);
    }
    U64 index = 0;
    List_ForEach(MemmyEval_AddressNode, node, list, link)
    {
        addresses[index++] = node->address;
    }
    return (MemmyEval_Value){
        .kind = MemmyEval_ValueKind_AddressList,
        .addresses = addresses,
        .address_count = list->count,
    };
}

static MemmyEval_Value MemmyEval_RangeList_FromList(Arena *arena, List *list)
{
    Memmy_Range *ranges = 0;
    if (list->count != 0)
    {
        ranges = Arena_PushArrayNoZero(arena, Memmy_Range, list->count);
    }
    U64 index = 0;
    List_ForEach(MemmyEval_RangeNode, node, list, link)
    {
        ranges[index++] = node->range;
    }
    return (MemmyEval_Value){
        .kind = MemmyEval_ValueKind_RangeList,
        .ranges = ranges,
        .range_count = list->count,
    };
}

static Memmy_Status MemmyEval_Transform_ListKindForValue(MemmyEval_Value value, MemmyEval_ValueKind *out_kind,
                                                         Memmy_Error *error)
{
    if (value.kind == MemmyEval_ValueKind_Address || value.kind == MemmyEval_ValueKind_AddressList)
    {
        *out_kind = MemmyEval_ValueKind_AddressList;
        return Memmy_Status_Ok;
    }
    if (value.kind == MemmyEval_ValueKind_Range || value.kind == MemmyEval_ValueKind_RangeList)
    {
        *out_kind = MemmyEval_ValueKind_RangeList;
        return Memmy_Status_Ok;
    }

    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                        String8_Lit("transform expression must produce address or range values"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status MemmyEval_Const_Add(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!AddI64Checked(a, b, out))
    {
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                            String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Const_Subtract(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!SubI64Checked(a, b, out))
    {
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                            String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Address_AddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
{
    if (!AddI64ToU64Checked(address, constant, out))
    {
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                            String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Address_SubConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
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
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                            String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static B32 MemmyEval_Value_IsAddressLike(MemmyEval_Value value)
{
    return value.kind == MemmyEval_ValueKind_Address || value.kind == MemmyEval_ValueKind_Range;
}

static Memmy_Status MemmyEval_Address_Diff(Memmy_Addr lhs, Memmy_Addr rhs, I64 *out, Memmy_Error *error)
{
    if (lhs >= rhs)
    {
        U64 magnitude = lhs - rhs;
        if (magnitude > (U64)I64_MAX)
        {
            MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
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
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                            String8_Lit("address difference overflow"));
        return Memmy_Status_Overflow;
    }
    *out = magnitude == limit ? I64_MIN : -(I64)magnitude;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Value_ApplyBinary(MemmyAst_ConstOp op, MemmyEval_Value lhs, MemmyEval_Value rhs,
                                         MemmyEval_Value *out, Memmy_Error *error)
{
    if (op == MemmyAst_ConstOp_Add || op == MemmyAst_ConstOp_Sub)
    {
        B32 lhs_address = MemmyEval_Value_IsAddressLike(lhs);
        B32 rhs_address = MemmyEval_Value_IsAddressLike(rhs);
        if (lhs_address && rhs_address)
        {
            if (op == MemmyAst_ConstOp_Add)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                    String8_Lit("cannot add two addresses"));
                return Memmy_Status_InvalidArgument;
            }

            Memmy_Addr lhs_addr = 0;
            Memmy_Addr rhs_addr = 0;
            Memmy_Status status = MemmyEval_Value_AsAddress(&lhs, &lhs_addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = MemmyEval_Value_AsAddress(&rhs, &rhs_addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            I64 diff = 0;
            status = MemmyEval_Address_Diff(lhs_addr, rhs_addr, &diff, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Const, .constant = diff};
            return Memmy_Status_Ok;
        }

        if (lhs_address)
        {
            I64 constant = 0;
            Memmy_Status status = MemmyEval_Value_AsConst(&rhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            Memmy_Addr address = 0;
            status = MemmyEval_Value_AsAddress(&lhs, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = op == MemmyAst_ConstOp_Add ? MemmyEval_Address_AddConst(address, constant, &address, error)
                                                : MemmyEval_Address_SubConst(address, constant, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (MemmyEval_Value){
                .kind = MemmyEval_ValueKind_Address,
                .address = address,
            };
            return Memmy_Status_Ok;
        }

        if (rhs_address)
        {
            if (op == MemmyAst_ConstOp_Sub)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                    String8_Lit("cannot subtract an address from a constant"));
                return Memmy_Status_InvalidArgument;
            }

            I64 constant = 0;
            Memmy_Status status = MemmyEval_Value_AsConst(&lhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            Memmy_Addr address = 0;
            status = MemmyEval_Value_AsAddress(&rhs, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = MemmyEval_Address_AddConst(address, constant, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            *out = (MemmyEval_Value){
                .kind = MemmyEval_ValueKind_Address,
                .address = address,
            };
            return Memmy_Status_Ok;
        }
    }

    I64 a = 0;
    I64 b = 0;
    Memmy_Status status = MemmyEval_Value_AsConst(&lhs, &a, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = MemmyEval_Value_AsConst(&rhs, &b, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    I64 result = 0;
    switch (op)
    {
    case MemmyAst_ConstOp_Add:
        status = MemmyEval_Const_Add(a, b, &result, error);
        break;
    case MemmyAst_ConstOp_Sub:
        status = MemmyEval_Const_Subtract(a, b, &result, error);
        break;
    case MemmyAst_ConstOp_Mul:
        if (!MulI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case MemmyAst_ConstOp_Div:
        if (b == 0)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                String8_Lit("division by zero"));
            return Memmy_Status_InvalidArgument;
        }
        if (!DivI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case MemmyAst_ConstOp_Mod:
        if (b == 0)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                String8_Lit("modulo by zero"));
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
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                            String8_Lit("constant arithmetic overflow"));
        return status;
    }
    if (status != Memmy_Status_Ok)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("unsupported arithmetic expression"));
        return status;
    }

    *out = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Const, .constant = result};
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Transform_Append(MemmyEval_Exec *exec, MemmyEval_Value value, List *addresses,
                                               List *ranges, MemmyEval_ValueKind *out_kind, Memmy_Error *error)
{
    MemmyEval_ValueKind value_kind = MemmyEval_ValueKind_Null;
    Memmy_Status status = MemmyEval_Transform_ListKindForValue(value, &value_kind, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (*out_kind == MemmyEval_ValueKind_Null)
    {
        *out_kind = value_kind;
    }
    else if (*out_kind != value_kind)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                            String8_Lit("transform expression produced mixed address and range values"));
        return Memmy_Status_InvalidArgument;
    }

    Arena *arena = exec->transient_arena;
    if (value.kind == MemmyEval_ValueKind_Address)
    {
        MemmyEval_AddressList_Push(arena, addresses, value.address);
    }
    else if (value.kind == MemmyEval_ValueKind_AddressList)
    {
        for (U64 i = 0; i < value.address_count; i++)
        {
            MemmyEval_AddressList_Push(arena, addresses, value.addresses[i]);
        }
    }
    else if (value.kind == MemmyEval_ValueKind_Range)
    {
        MemmyEval_RangeList_Push(arena, ranges, value.range);
    }
    else if (value.kind == MemmyEval_ValueKind_RangeList)
    {
        for (U64 i = 0; i < value.range_count; i++)
        {
            MemmyEval_RangeList_Push(arena, ranges, value.ranges[i]);
        }
    }

    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_List_Transform(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                      Memmy_Error *error)
{
    MemmyEval_Value list = {0};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &list, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (list.kind != MemmyEval_ValueKind_AddressList && list.kind != MemmyEval_ValueKind_RangeList)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                            String8_Lit("expected address list or range list"));
        return Memmy_Status_InvalidArgument;
    }

    U64 count = list.kind == MemmyEval_ValueKind_AddressList ? list.address_count : list.range_count;
    if (count == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_NotFound, String8_Lit("transform"),
                            String8_Lit("transform input list is empty"));
        return Memmy_Status_NotFound;
    }

    List addresses = {0}; // MemmyEval_AddressNode
    List ranges = {0};    // MemmyEval_RangeNode
    MemmyEval_ValueKind out_kind = MemmyEval_ValueKind_Null;
    B32 old_has_current_item = exec->has_current_item;
    MemmyEval_Value old_current_item = exec->current_item;

    for (U64 i = 0; i < count; i++)
    {
        exec->has_current_item = 1;
        if (list.kind == MemmyEval_ValueKind_AddressList)
        {
            exec->current_item = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Address, .address = list.addresses[i]};
        }
        else
        {
            exec->current_item = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Range, .range = list.ranges[i]};
        }

        MemmyEval_Value item_result = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &item_result, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
        status = MemmyEval_Transform_Append(exec, item_result, &addresses, &ranges, &out_kind, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
    }

    exec->has_current_item = old_has_current_item;
    exec->current_item = old_current_item;
    if (out_kind == MemmyEval_ValueKind_RangeList)
    {
        *out = MemmyEval_RangeList_FromList(exec->out_arena, &ranges);
    }
    else
    {
        *out = MemmyEval_AddressList_FromList(exec->out_arena, &addresses);
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Expr_EvalValue(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                      Memmy_Error *error)
{
    MemmyEval_Env *env = exec->env;
    (void)env;
    if (expr->kind == MemmyAst_NodeKind_ConstArithmetic)
    {
        if (expr->op == MemmyAst_ConstOp_None)
        {
            *out = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Const, .constant = expr->value};
            return Memmy_Status_Ok;
        }

        MemmyEval_Value lhs = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == MemmyAst_ConstOp_Pos || expr->op == MemmyAst_ConstOp_Neg)
        {
            I64 constant = 0;
            status = MemmyEval_Value_AsConst(&lhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (expr->op == MemmyAst_ConstOp_Neg && !SubI64Checked(0, constant, &constant))
            {
                MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                                    String8_Lit("constant arithmetic overflow"));
                return Memmy_Status_Overflow;
            }
            *out = (MemmyEval_Value){.kind = MemmyEval_ValueKind_Const, .constant = constant};
            return Memmy_Status_Ok;
        }

        MemmyEval_Value rhs = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return MemmyEval_Value_ApplyBinary(expr->op, lhs, rhs, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_Variable)
    {
        return MemmyEval_Env_Find(exec->out_arena, env, expr->name, out);
    }
    if (expr->kind == MemmyAst_NodeKind_CurrentItem)
    {
        if (!exec->has_current_item)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                                String8_Lit("current item is only available inside transforms"));
            return Memmy_Status_InvalidArgument;
        }
        *out = exec->current_item;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ListTransform)
    {
        return MemmyEval_List_Transform(exec, expr, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_Address)
    {
        if (expr->value_expr != 0)
        {
            MemmyEval_Value value = {0};
            Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->value_expr, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            I64 constant = 0;
            status = MemmyEval_Value_AsConst(&value, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (constant < 0)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                                    String8_Lit("address cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            *out = (MemmyEval_Value){
                .kind = MemmyEval_ValueKind_Address,
                .address = (Memmy_Addr)constant,
            };
            return Memmy_Status_Ok;
        }

        MemmyEval_Value base = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = MemmyEval_Value_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_Value offset_value = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &offset_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 offset = 0;
        status = MemmyEval_Value_AsConst(&offset_value, &offset, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = MemmyEval_Address_AddConst(address, offset, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (MemmyEval_Value){
            .kind = MemmyEval_ValueKind_Address,
            .address = address,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Range)
    {
        MemmyEval_Value start_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &start_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr start = 0;
        status = MemmyEval_Value_AsAddress(&start_value, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        MemmyEval_Value rhs = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        if (expr->range_is_sized)
        {
            I64 size = 0;
            status = MemmyEval_Value_AsConst(&rhs, &size, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (size < 0)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                                    String8_Lit("range size cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            status = Memmy_Range_FromStartLength(start, (Memmy_Size)size, &range, error);
        }
        else
        {
            Memmy_Addr end = 0;
            status = MemmyEval_Value_AsAddress(&rhs, &end, error);
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

        *out = (MemmyEval_Value){
            .kind = MemmyEval_ValueKind_Range,
            .range = range,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Index)
    {
        MemmyEval_Value list = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &list, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (list.kind != MemmyEval_ValueKind_AddressList && list.kind != MemmyEval_ValueKind_RangeList)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("index"),
                                String8_Lit("expected address list or range list"));
            return Memmy_Status_InvalidArgument;
        }

        MemmyEval_Value index_value = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &index_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 index = 0;
        status = MemmyEval_Value_AsConst(&index_value, &index, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        U64 count = list.kind == MemmyEval_ValueKind_AddressList ? list.address_count : list.range_count;
        if (index < 0 || (U64)index >= count)
        {
            MemmyEval_Error_Set(error, Memmy_Status_NotFound, String8_Lit("index"),
                                String8_Lit("list index out of range"));
            return Memmy_Status_NotFound;
        }

        if (list.kind == MemmyEval_ValueKind_AddressList)
        {
            *out = (MemmyEval_Value){
                .kind = MemmyEval_ValueKind_Address,
                .address = list.addresses[index],
            };
        }
        else
        {
            *out = (MemmyEval_Value){
                .kind = MemmyEval_ValueKind_Range,
                .range = list.ranges[index],
            };
        }
        return Memmy_Status_Ok;
    }
    MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
