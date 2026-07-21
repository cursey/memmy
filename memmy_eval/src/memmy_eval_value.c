#include "memmy_eval_internal.h"

#include "base.h"

typedef struct MemmyEval_ValueNode MemmyEval_ValueNode;
struct MemmyEval_ValueNode
{
    ListLink link;
    Memmy_Value value;
};

static Memmy_Value MemmyEval_I64(I64 value)
{
    return (Memmy_Value){.type = Memmy_Type_I64, .signed_integer = value};
}

Memmy_Status MemmyEval_Value_AsI64(Memmy_Value const *value, I64 *out, Memmy_Error *error)
{
    if (value == 0 || out == 0 || !Memmy_Type_IsInteger(value->type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("expected integer value"));
        return Memmy_Status_InvalidArgument;
    }
    if (!value->type.integer.is_signed && value->unsigned_integer > (U64)I64_MAX)
    {
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("integer does not fit i64"));
        return Memmy_Status_Overflow;
    }
    *out = value->type.integer.is_signed ? value->signed_integer : (I64)value->unsigned_integer;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Value_AsAddress(Memmy_Value const *value, Memmy_Addr *out, Memmy_Error *error)
{
    if (value != 0 && out != 0 && Memmy_Type_IsAddress(value->type))
    {
        *out = value->address;
        return Memmy_Status_Ok;
    }
    if (value != 0 && out != 0 && Memmy_Type_IsRange(value->type))
    {
        *out = value->range.start;
        return Memmy_Status_Ok;
    }
    MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("expected address or range value"));
    return Memmy_Status_InvalidArgument;
}

static U64 MemmyEval_IntegerMask(U32 bits)
{
    return bits == 64 ? U64_MAX : (1ull << bits) - 1;
}

static I64 MemmyEval_IntegerMin(U32 bits)
{
    return bits == 64 ? I64_MIN : -(I64)(1ull << (bits - 1));
}

static I64 MemmyEval_IntegerMax(U32 bits)
{
    return bits == 64 ? I64_MAX : (I64)((1ull << (bits - 1)) - 1);
}

static Memmy_Type MemmyEval_IntegerPromote(Memmy_Type type)
{
    return type.integer.bit_count < 32 ? Memmy_Type_I32 : type;
}

static Memmy_Type MemmyEval_IntegerCommonType(Memmy_Type a, Memmy_Type b)
{
    a = MemmyEval_IntegerPromote(a);
    b = MemmyEval_IntegerPromote(b);
    if (a.integer.is_signed == b.integer.is_signed)
    {
        return a.integer.bit_count >= b.integer.bit_count ? a : b;
    }
    Memmy_Type unsigned_type = a.integer.is_signed ? b : a;
    Memmy_Type signed_type = a.integer.is_signed ? a : b;
    if (unsigned_type.integer.bit_count >= signed_type.integer.bit_count)
    {
        return unsigned_type;
    }
    return signed_type;
}

static Memmy_Status MemmyEval_IntegerConvert(Memmy_Value value, Memmy_Type type, Memmy_Value *out, Memmy_Error *error)
{
    Memmy_Value result = {.type = type};
    if (type.integer.is_signed)
    {
        if (value.type.integer.is_signed)
        {
            result.signed_integer = value.signed_integer;
        }
        else if (value.unsigned_integer <= (U64)MemmyEval_IntegerMax(type.integer.bit_count))
        {
            result.signed_integer = (I64)value.unsigned_integer;
        }
        else
        {
            MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                                String8_Lit("integer conversion overflow"));
            return Memmy_Status_Overflow;
        }
    }
    else
    {
        U64 raw = value.type.integer.is_signed ? (U64)value.signed_integer : value.unsigned_integer;
        result.unsigned_integer = raw & MemmyEval_IntegerMask(type.integer.bit_count);
    }
    *out = result;
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_IntegerBinary(MemmyAst_ConstOp op, Memmy_Value lhs, Memmy_Value rhs, Memmy_Value *out,
                                            Memmy_Error *error)
{
    if (!Memmy_Type_IsInteger(lhs.type) || !Memmy_Type_IsInteger(rhs.type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("integer arithmetic requires integer operands"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Type type = MemmyEval_IntegerCommonType(lhs.type, rhs.type);
    Memmy_Value a = {0};
    Memmy_Value b = {0};
    Memmy_Status status = MemmyEval_IntegerConvert(lhs, type, &a, error);
    if (status == Memmy_Status_Ok)
    {
        status = MemmyEval_IntegerConvert(rhs, type, &b, error);
    }
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value result = {.type = type};
    if (!type.integer.is_signed)
    {
        U64 mask = MemmyEval_IntegerMask(type.integer.bit_count);
        switch (op)
        {
        case MemmyAst_ConstOp_Add:
            result.unsigned_integer = (a.unsigned_integer + b.unsigned_integer) & mask;
            break;
        case MemmyAst_ConstOp_Sub:
            result.unsigned_integer = (a.unsigned_integer - b.unsigned_integer) & mask;
            break;
        case MemmyAst_ConstOp_Mul:
            result.unsigned_integer = (a.unsigned_integer * b.unsigned_integer) & mask;
            break;
        case MemmyAst_ConstOp_Div:
        case MemmyAst_ConstOp_Mod:
            if (b.unsigned_integer == 0)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                    String8_Lit("division by zero"));
                return Memmy_Status_InvalidArgument;
            }
            result.unsigned_integer = op == MemmyAst_ConstOp_Div ? a.unsigned_integer / b.unsigned_integer
                                                                 : a.unsigned_integer % b.unsigned_integer;
            break;
        default:
            return Memmy_Status_InvalidArgument;
        }
    }
    else
    {
        I64 value = 0;
        B32 ok = 0;
        if (op == MemmyAst_ConstOp_Add)
        {
            ok = AddI64Checked(a.signed_integer, b.signed_integer, &value);
        }
        else if (op == MemmyAst_ConstOp_Sub)
        {
            ok = SubI64Checked(a.signed_integer, b.signed_integer, &value);
        }
        else if (op == MemmyAst_ConstOp_Mul)
        {
            ok = MulI64Checked(a.signed_integer, b.signed_integer, &value);
        }
        else if (op == MemmyAst_ConstOp_Div)
        {
            ok = DivI64Checked(a.signed_integer, b.signed_integer, &value);
        }
        else if (op == MemmyAst_ConstOp_Mod)
        {
            ok = ModI64Checked(a.signed_integer, b.signed_integer, &value);
        }
        if (!ok || value < MemmyEval_IntegerMin(type.integer.bit_count) ||
            value > MemmyEval_IntegerMax(type.integer.bit_count))
        {
            Memmy_Status error_status =
                b.signed_integer == 0 && (op == MemmyAst_ConstOp_Div || op == MemmyAst_ConstOp_Mod)
                    ? Memmy_Status_InvalidArgument
                    : Memmy_Status_Overflow;
            MemmyEval_Error_Set(error, error_status, String8_Lit("expr"),
                                error_status == Memmy_Status_Overflow ? String8_Lit("integer arithmetic overflow")
                                                                      : String8_Lit("division by zero"));
            return error_status;
        }
        result.signed_integer = value;
    }
    *out = result;
    return Memmy_Status_Ok;
}

static B32 MemmyEval_Value_IsAddressLike(Memmy_Value value)
{
    return Memmy_Type_IsAddress(value.type) || Memmy_Type_IsRange(value.type);
}

static Memmy_Status MemmyEval_AddressDifference(Memmy_Addr lhs, Memmy_Addr rhs, I64 *out, Memmy_Error *error)
{
    if (lhs >= rhs)
    {
        U64 difference = lhs - rhs;
        if (difference > (U64)I64_MAX)
        {
            goto overflow;
        }
        *out = (I64)difference;
    }
    else
    {
        U64 difference = rhs - lhs;
        if (difference > (U64)I64_MAX + 1)
        {
            goto overflow;
        }
        *out = difference == (U64)I64_MAX + 1 ? I64_MIN : -(I64)difference;
    }
    return Memmy_Status_Ok;
overflow:
    MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address difference overflow"));
    return Memmy_Status_Overflow;
}

static Memmy_Status MemmyEval_AddressOffset(Memmy_Addr address, Memmy_Value integer, B32 subtract, Memmy_Addr *out,
                                            Memmy_Error *error)
{
    if (!Memmy_Type_IsInteger(integer.type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                            String8_Lit("address offset must be an integer"));
        return Memmy_Status_InvalidArgument;
    }
    B32 ok = 0;
    if (integer.type.integer.is_signed)
    {
        I64 offset = integer.signed_integer;
        if (subtract)
        {
            if (offset >= 0)
            {
                ok = SubU64Checked(address, (U64)offset, out);
            }
            else
            {
                ok = AddU64Checked(address, (U64)(-(offset + 1)) + 1, out);
            }
        }
        else
        {
            ok = AddI64ToU64Checked(address, offset, out);
        }
    }
    else if (subtract)
    {
        ok = SubU64Checked(address, integer.unsigned_integer, out);
    }
    else
    {
        ok = AddU64Checked(address, integer.unsigned_integer, out);
    }
    if (!ok)
    {
        MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                            String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Value_ApplyBinary(MemmyAst_ConstOp op, Memmy_Value lhs, Memmy_Value rhs, Memmy_Value *out,
                                         Memmy_Error *error)
{
    B32 lhs_address = MemmyEval_Value_IsAddressLike(lhs);
    B32 rhs_address = MemmyEval_Value_IsAddressLike(rhs);
    if (lhs_address || rhs_address)
    {
        if (lhs_address && rhs_address && op == MemmyAst_ConstOp_Sub)
        {
            I64 difference = 0;
            Memmy_Status status = MemmyEval_AddressDifference(
                Memmy_Type_IsRange(lhs.type) ? lhs.range.start : lhs.address,
                Memmy_Type_IsRange(rhs.type) ? rhs.range.start : rhs.address, &difference, error);
            if (status == Memmy_Status_Ok)
            {
                *out = MemmyEval_I64(difference);
            }
            return status;
        }
        Memmy_Value address_value = lhs_address ? lhs : rhs;
        Memmy_Value offset = lhs_address ? rhs : lhs;
        if ((lhs_address && rhs_address) || op != MemmyAst_ConstOp_Add && !(lhs_address && op == MemmyAst_ConstOp_Sub))
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                String8_Lit("invalid address arithmetic"));
            return Memmy_Status_InvalidArgument;
        }
        Memmy_Addr address = Memmy_Type_IsRange(address_value.type) ? address_value.range.start : address_value.address;
        Memmy_Status status = MemmyEval_AddressOffset(address, offset, op == MemmyAst_ConstOp_Sub, &address, error);
        if (status == Memmy_Status_Ok)
        {
            *out = (Memmy_Value){.type = Memmy_Type_Address, .address = address};
        }
        return status;
    }
    return MemmyEval_IntegerBinary(op, lhs, rhs, out, error);
}

static Memmy_Value MemmyEval_ListItem(Memmy_Value list, U64 index)
{
    Memmy_Type type = *list.type.list.element_type;
    Memmy_Value result = {.type = type};
    if (type.kind == Memmy_TypeKind_Integer)
    {
        if (type.integer.is_signed)
        {
            result.signed_integer = list.list.signed_integers[index];
        }
        else
        {
            result.unsigned_integer = list.list.unsigned_integers[index];
        }
    }
    else if (type.kind == Memmy_TypeKind_Float)
    {
        result.floating_bits =
            type.floating.bit_count == 32 ? list.list.floating32_bits[index] : list.list.floating64_bits[index];
    }
    else if (type.kind == Memmy_TypeKind_Address)
    {
        result.address = list.list.addresses[index];
    }
    else if (type.kind == Memmy_TypeKind_Range)
    {
        result.range = list.list.ranges[index];
    }
    else if (type.kind == Memmy_TypeKind_String)
    {
        result.string = list.list.strings[index];
    }
    return result;
}

static B32 MemmyEval_Type_IsAddressLike(Memmy_Type type)
{
    return Memmy_Type_IsAddress(type) || Memmy_Type_IsRange(type);
}

static Memmy_Status MemmyEval_Type_ConversionCheck(Memmy_Type source, Memmy_Type destination, B32 address_is_read,
                                                   Memmy_Error *error)
{
    B32 supported = Memmy_Type_Eq(source, destination) ||
                    ((Memmy_Type_IsInteger(source) || Memmy_Type_IsFloat(source)) &&
                     (Memmy_Type_IsInteger(destination) || Memmy_Type_IsFloat(destination))) ||
                    (Memmy_Type_IsString(source) && Memmy_Type_IsString(destination)) ||
                    (address_is_read && MemmyEval_Type_IsAddressLike(source));
    if (!supported)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("type"),
                            String8_Lit("unsupported value conversion"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Type_ResolveBinary(MemmyAst_ConstOp op, Memmy_Type lhs, Memmy_Type rhs, Memmy_Type *out,
                                                 Memmy_Error *error)
{
    B32 lhs_address = MemmyEval_Type_IsAddressLike(lhs);
    B32 rhs_address = MemmyEval_Type_IsAddressLike(rhs);
    if (lhs_address || rhs_address)
    {
        if (lhs_address && rhs_address && op == MemmyAst_ConstOp_Sub)
        {
            *out = Memmy_Type_I64;
            return Memmy_Status_Ok;
        }
        B32 valid = lhs_address != rhs_address && Memmy_Type_IsInteger(lhs_address ? rhs : lhs) &&
                    (op == MemmyAst_ConstOp_Add || lhs_address && op == MemmyAst_ConstOp_Sub);
        if (valid)
        {
            *out = Memmy_Type_Address;
            return Memmy_Status_Ok;
        }
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("invalid address arithmetic"));
        return Memmy_Status_InvalidArgument;
    }
    if (!Memmy_Type_IsInteger(lhs) || !Memmy_Type_IsInteger(rhs))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("integer arithmetic requires integer operands"));
        return Memmy_Status_InvalidArgument;
    }
    *out = MemmyEval_IntegerCommonType(lhs, rhs);
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_Expr_ResolveType(MemmyEval_Exec *exec, MemmyAst_Node const *expr, B32 has_current_item,
                                               Memmy_Type current_item_type, Memmy_Type *out, Memmy_Error *error)
{
    *out = (Memmy_Type){0};
    if (expr == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    if (expr->kind == MemmyAst_NodeKind_Nil)
    {
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ConstArithmetic)
    {
        if (expr->op == MemmyAst_ConstOp_None)
        {
            *out = Memmy_Type_I64;
            return Memmy_Status_Ok;
        }
        Memmy_Type lhs = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == MemmyAst_ConstOp_Pos || expr->op == MemmyAst_ConstOp_Neg)
        {
            if (!Memmy_Type_IsInteger(lhs))
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                                    String8_Lit("unary arithmetic requires an integer"));
                return Memmy_Status_InvalidArgument;
            }
            *out = MemmyEval_IntegerPromote(lhs);
            return Memmy_Status_Ok;
        }
        Memmy_Type rhs = {0};
        status = MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &rhs, error);
        return status == Memmy_Status_Ok ? MemmyEval_Type_ResolveBinary(expr->op, lhs, rhs, out, error) : status;
    }
    if (expr->kind == MemmyAst_NodeKind_FloatLiteral)
    {
        *out = Memmy_Type_F64;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_StringLiteral)
    {
        *out = Memmy_Type_Str;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Variable)
    {
        return MemmyEval_Env_TypeFind(exec->env, expr->name, out);
    }
    if (expr->kind == MemmyAst_NodeKind_CurrentItem)
    {
        if (!has_current_item)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("flow"),
                                String8_Lit("current flow input is only available inside flow expressions"));
            return Memmy_Status_InvalidArgument;
        }
        *out = current_item_type;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ListTransform)
    {
        Memmy_Type input = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &input, error);
        if (status != Memmy_Status_Ok || Memmy_Type_IsNull(input))
        {
            return status;
        }
        if (!Memmy_Type_IsList(input))
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                                String8_Lit("transform requires a list"));
            return Memmy_Status_InvalidArgument;
        }
        Memmy_Type result = {0};
        status = MemmyEval_Expr_ResolveType(exec, expr->rhs, 1, *input.list.element_type, &result, error);
        Memmy_Type element_type = Memmy_Type_IsList(result) ? *result.list.element_type : result;
        if (status != Memmy_Status_Ok || Memmy_Type_IsNull(element_type) || Memmy_Type_IsList(element_type) ||
            !Memmy_Type_IsValid(element_type))
        {
            if (status == Memmy_Status_Ok)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                                    String8_Lit("transform result must have a scalar type"));
                status = Memmy_Status_InvalidArgument;
            }
            return status;
        }
        return Memmy_Type_ListCreate(exec->transient_arena, element_type, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_ValuePipe)
    {
        Memmy_Type input = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &input, error);
        return status == Memmy_Status_Ok ? MemmyEval_Expr_ResolveType(exec, expr->rhs, 1, input, out, error) : status;
    }
    if (expr->kind == MemmyAst_NodeKind_Address)
    {
        Memmy_Type integer = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->value_expr, has_current_item, current_item_type, &integer, error);
        if (status != Memmy_Status_Ok || !Memmy_Type_IsInteger(integer))
        {
            return status == Memmy_Status_Ok ? Memmy_Status_InvalidArgument : status;
        }
        *out = Memmy_Type_Address;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Range)
    {
        Memmy_Type start = {0};
        Memmy_Type end_or_size = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &start, error);
        if (status == Memmy_Status_Ok)
        {
            status =
                MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &end_or_size, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        B32 valid =
            MemmyEval_Type_IsAddressLike(start) &&
            (expr->range_is_sized ? Memmy_Type_IsInteger(end_or_size) : MemmyEval_Type_IsAddressLike(end_or_size));
        if (!valid)
        {
            return Memmy_Status_InvalidArgument;
        }
        *out = Memmy_Type_Range;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Index)
    {
        Memmy_Type list = {0};
        Memmy_Type index = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &list, error);
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &index, error);
        }
        if (status != Memmy_Status_Ok || !Memmy_Type_IsList(list) || !Memmy_Type_IsInteger(index))
        {
            return status == Memmy_Status_Ok ? Memmy_Status_InvalidArgument : status;
        }
        *out = *list.list.element_type;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Target || expr->kind == MemmyAst_NodeKind_ProcessRange)
    {
        *out = Memmy_Type_Range;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Function || expr->kind == MemmyAst_NodeKind_ObjectBase ||
        expr->kind == MemmyAst_NodeKind_Deref)
    {
        Memmy_Type base = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &base, error);
        if (status != Memmy_Status_Ok || !MemmyEval_Type_IsAddressLike(base))
        {
            return status == Memmy_Status_Ok ? Memmy_Status_InvalidArgument : status;
        }
        if (expr->kind == MemmyAst_NodeKind_Deref && expr->rhs != 0)
        {
            Memmy_Type offset = {0};
            status = MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &offset, error);
            if (status != Memmy_Status_Ok || !Memmy_Type_IsInteger(offset))
            {
                return status == Memmy_Status_Ok ? Memmy_Status_InvalidArgument : status;
            }
        }
        *out = expr->kind == MemmyAst_NodeKind_Function ? Memmy_Type_Range : Memmy_Type_Address;
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_TypedRead)
    {
        Memmy_Type source = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &source, error);
        Memmy_Type destination = {0};
        if (status == Memmy_Status_Ok)
        {
            status = Memmy_Type_Parse(expr->type_name, &destination, error);
        }
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Type_ConversionCheck(source, destination, 1, error);
        }
        if (status == Memmy_Status_Ok)
        {
            *out = destination;
        }
        return status;
    }
    if (expr->kind == MemmyAst_NodeKind_PatternScan || expr->kind == MemmyAst_NodeKind_ValueScan ||
        expr->kind == MemmyAst_NodeKind_ReferenceScan || expr->kind == MemmyAst_NodeKind_DisasmScan)
    {
        Memmy_Type range = {0};
        Memmy_Status status =
            MemmyEval_Expr_ResolveType(exec, expr->lhs, has_current_item, current_item_type, &range, error);
        if (status != Memmy_Status_Ok || !Memmy_Type_IsRange(range))
        {
            return status == Memmy_Status_Ok ? Memmy_Status_InvalidArgument : status;
        }
        if (expr->kind == MemmyAst_NodeKind_ValueScan)
        {
            Memmy_Type source = {0};
            Memmy_Type destination = {0};
            status = MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &source, error);
            if (status == Memmy_Status_Ok)
            {
                status = Memmy_Type_Parse(expr->type_name, &destination, error);
            }
            if (status == Memmy_Status_Ok)
            {
                status = MemmyEval_Type_ConversionCheck(source, destination, 0, error);
            }
        }
        else if (expr->kind == MemmyAst_NodeKind_ReferenceScan)
        {
            Memmy_Type target = {0};
            status = MemmyEval_Expr_ResolveType(exec, expr->rhs, has_current_item, current_item_type, &target, error);
            if (status == Memmy_Status_Ok && !MemmyEval_Type_IsAddressLike(target))
            {
                status = Memmy_Status_InvalidArgument;
            }
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Type_ListCreate(exec->transient_arena, Memmy_Type_Address, out, error);
    }
    return Memmy_Status_Unsupported;
}

static Memmy_Status MemmyEval_CompactList(Arena *arena, Memmy_Type element_type, List *items, Memmy_Value *out,
                                          Memmy_Error *error)
{
    Memmy_Type list_type = {0};
    Memmy_Status status = Memmy_Type_ListCreate(arena, element_type, &list_type, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    Memmy_Value result = {.type = list_type, .list.count = items->count};
    if (element_type.kind == Memmy_TypeKind_Integer)
    {
        result.list.unsigned_integers = Arena_PushArrayNoZero(arena, U64, items->count);
    }
    else if (element_type.kind == Memmy_TypeKind_Float && element_type.floating.bit_count == 32)
    {
        result.list.floating32_bits = Arena_PushArrayNoZero(arena, U32, items->count);
    }
    else if (element_type.kind == Memmy_TypeKind_Float)
    {
        result.list.floating64_bits = Arena_PushArrayNoZero(arena, U64, items->count);
    }
    else if (element_type.kind == Memmy_TypeKind_Address)
    {
        result.list.addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, items->count);
    }
    else if (element_type.kind == Memmy_TypeKind_Range)
    {
        result.list.ranges = Arena_PushArrayNoZero(arena, Memmy_Range, items->count);
    }
    else if (element_type.kind == Memmy_TypeKind_String)
    {
        result.list.strings = Arena_PushArrayNoZero(arena, String8, items->count);
    }
    U64 index = 0;
    List_ForEach(MemmyEval_ValueNode, node, items, link)
    {
        Memmy_Value value = node->value;
        if (element_type.kind == Memmy_TypeKind_Integer && element_type.integer.is_signed)
        {
            result.list.signed_integers[index] = value.signed_integer;
        }
        else if (element_type.kind == Memmy_TypeKind_Integer)
        {
            result.list.unsigned_integers[index] = value.unsigned_integer;
        }
        else if (element_type.kind == Memmy_TypeKind_Float && element_type.floating.bit_count == 32)
        {
            result.list.floating32_bits[index] = (U32)value.floating_bits;
        }
        else if (element_type.kind == Memmy_TypeKind_Float)
        {
            result.list.floating64_bits[index] = value.floating_bits;
        }
        else if (element_type.kind == Memmy_TypeKind_Address)
        {
            result.list.addresses[index] = value.address;
        }
        else if (element_type.kind == Memmy_TypeKind_Range)
        {
            result.list.ranges[index] = value.range;
        }
        else if (element_type.kind == Memmy_TypeKind_String)
        {
            result.list.strings[index] = String8_Copy(arena, value.string);
        }
        index++;
    }
    *out = result;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_List_Transform(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out,
                                      Memmy_Error *error)
{
    Memmy_Value input = {0};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &input, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (Memmy_Type_IsNull(input.type))
    {
        *out = (Memmy_Value){0};
        return Memmy_Status_Ok;
    }
    if (!Memmy_Type_IsList(input.type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                            String8_Lit("transform requires a list"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Type resolved = {0};
    status = MemmyEval_Expr_ResolveType(exec, expr->rhs, 1, *input.type.list.element_type, &resolved, error);
    Memmy_Type output_type = Memmy_Type_IsList(resolved) ? *resolved.list.element_type : resolved;
    if (status != Memmy_Status_Ok || Memmy_Type_IsNull(output_type) || Memmy_Type_IsList(output_type) ||
        !Memmy_Type_IsValid(output_type))
    {
        if (status == Memmy_Status_Ok)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                                String8_Lit("transform result must have a scalar type"));
            status = Memmy_Status_InvalidArgument;
        }
        return status;
    }
    B32 old_has_item = exec->has_current_item;
    Memmy_Value old_item = exec->current_item;
    List items = {0}; // MemmyEval_ValueNode
    for (U64 i = 0; i < input.list.count; i++)
    {
        exec->has_current_item = 1;
        exec->current_item = MemmyEval_ListItem(input, i);
        Memmy_Value item = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &item, error);
        if (status != Memmy_Status_Ok || Memmy_Type_IsNull(item.type))
        {
            status = Memmy_Status_Ok;
            continue;
        }
        U64 count = Memmy_Type_IsList(item.type) ? item.list.count : 1;
        Memmy_Type item_type = Memmy_Type_IsList(item.type) ? *item.type.list.element_type : item.type;
        if (Memmy_Type_IsList(item_type) || Memmy_Type_IsNull(item_type))
        {
            status = Memmy_Status_InvalidArgument;
            break;
        }
        if (!Memmy_Type_Eq(output_type, item_type))
        {
            status = Memmy_Status_InvalidArgument;
            break;
        }
        for (U64 j = 0; j < count; j++)
        {
            MemmyEval_ValueNode *node = Arena_PushStruct(exec->transient_arena, MemmyEval_ValueNode);
            node->value = Memmy_Type_IsList(item.type) ? MemmyEval_ListItem(item, j) : item;
            List_PushBack(&items, &node->link);
        }
    }
    exec->has_current_item = old_has_item;
    exec->current_item = old_item;
    if (status != Memmy_Status_Ok)
    {
        MemmyEval_Error_Set(error, status, String8_Lit("transform"), String8_Lit("incompatible transform result"));
        return status;
    }
    return MemmyEval_CompactList(exec->out_arena, output_type, &items, out, error);
}

Memmy_Status MemmyEval_Value_Pipe(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out, Memmy_Error *error)
{
    Memmy_Value input = {0};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &input, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    B32 old_has_item = exec->has_current_item;
    Memmy_Value old_item = exec->current_item;
    exec->has_current_item = 1;
    exec->current_item = input;
    status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, out, error);
    exec->has_current_item = old_has_item;
    exec->current_item = old_item;
    return status;
}

static Memmy_Status MemmyEval_IntegerUnary(MemmyAst_ConstOp op, Memmy_Value value, Memmy_Value *out, Memmy_Error *error)
{
    if (!Memmy_Type_IsInteger(value.type))
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                            String8_Lit("unary arithmetic requires an integer"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Type type = MemmyEval_IntegerPromote(value.type);
    Memmy_Status status = MemmyEval_IntegerConvert(value, type, out, error);
    if (status != Memmy_Status_Ok || op == MemmyAst_ConstOp_Pos)
    {
        return status;
    }
    if (type.integer.is_signed)
    {
        if (out->signed_integer == MemmyEval_IntegerMin(type.integer.bit_count))
        {
            MemmyEval_Error_Set(error, Memmy_Status_Overflow, String8_Lit("expr"),
                                String8_Lit("integer arithmetic overflow"));
            return Memmy_Status_Overflow;
        }
        out->signed_integer = -out->signed_integer;
    }
    else
    {
        out->unsigned_integer = (0 - out->unsigned_integer) & MemmyEval_IntegerMask(type.integer.bit_count);
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Expr_EvalValue(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out,
                                      Memmy_Error *error)
{
    if (expr->kind == MemmyAst_NodeKind_Nil)
    {
        *out = (Memmy_Value){0};
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_ConstArithmetic)
    {
        if (expr->op == MemmyAst_ConstOp_None)
        {
            *out = MemmyEval_I64(expr->value);
            return Memmy_Status_Ok;
        }
        Memmy_Value lhs = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == MemmyAst_ConstOp_Pos || expr->op == MemmyAst_ConstOp_Neg)
        {
            return MemmyEval_IntegerUnary(expr->op, lhs, out, error);
        }
        Memmy_Value rhs = {0};
        status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &rhs, error);
        return status == Memmy_Status_Ok ? MemmyEval_Value_ApplyBinary(expr->op, lhs, rhs, out, error) : status;
    }
    if (expr->kind == MemmyAst_NodeKind_FloatLiteral)
    {
        *out = (Memmy_Value){.type = Memmy_Type_F64, .floating_bits = expr->floating_bits};
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_StringLiteral)
    {
        *out = (Memmy_Value){.type = Memmy_Type_Str, .string = String8_Copy(exec->out_arena, expr->string)};
        return Memmy_Status_Ok;
    }
    if (expr->kind == MemmyAst_NodeKind_Variable)
    {
        return MemmyEval_Env_Find(exec->out_arena, exec->env, expr->name, out);
    }
    if (expr->kind == MemmyAst_NodeKind_CurrentItem)
    {
        if (!exec->has_current_item)
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("flow"),
                                String8_Lit("current flow input is only available inside flow expressions"));
            return Memmy_Status_InvalidArgument;
        }
        return Memmy_Value_Copy(exec->out_arena, &exec->current_item, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_ListTransform)
    {
        return MemmyEval_List_Transform(exec, expr, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_ValuePipe)
    {
        return MemmyEval_Value_Pipe(exec, expr, out, error);
    }
    if (expr->kind == MemmyAst_NodeKind_Address)
    {
        if (expr->value_expr != 0)
        {
            Memmy_Value integer = {0};
            Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->value_expr, &integer, error);
            if (status != Memmy_Status_Ok || !Memmy_Type_IsInteger(integer.type))
            {
                return status != Memmy_Status_Ok ? status : Memmy_Status_InvalidArgument;
            }
            if (integer.type.integer.is_signed && integer.signed_integer < 0)
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                                    String8_Lit("address cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            *out = (Memmy_Value){.type = Memmy_Type_Address,
                                 .address = integer.type.integer.is_signed ? (U64)integer.signed_integer
                                                                           : integer.unsigned_integer};
            return Memmy_Status_Ok;
        }
        Memmy_Value base = {0};
        Memmy_Value offset = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &base, error);
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &offset, error);
        }
        return status == Memmy_Status_Ok ? MemmyEval_Value_ApplyBinary(MemmyAst_ConstOp_Add, base, offset, out, error)
                                         : status;
    }
    if (expr->kind == MemmyAst_NodeKind_Range)
    {
        Memmy_Value start_value = {0};
        Memmy_Value rhs = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &start_value, error);
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &rhs, error);
        }
        Memmy_Addr start = 0;
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Value_AsAddress(&start_value, &start, error);
        }
        Memmy_Range range = {0};
        if (status == Memmy_Status_Ok && expr->range_is_sized)
        {
            if (!Memmy_Type_IsInteger(rhs.type) || (rhs.type.integer.is_signed && rhs.signed_integer < 0))
            {
                MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                                    String8_Lit("range size must be a nonnegative integer"));
                status = Memmy_Status_InvalidArgument;
            }
            if (status == Memmy_Status_Ok)
            {
                Memmy_Addr end = 0;
                status = MemmyEval_AddressOffset(start, rhs, 0, &end, error);
                if (status == Memmy_Status_Ok)
                {
                    status = Memmy_Range_FromStartEnd(start, end, &range, error);
                }
            }
        }
        else if (status == Memmy_Status_Ok)
        {
            Memmy_Addr end = 0;
            status = MemmyEval_Value_AsAddress(&rhs, &end, error);
            if (status == Memmy_Status_Ok)
            {
                status = Memmy_Range_FromStartEnd(start, end, &range, error);
            }
        }
        if (status == Memmy_Status_Ok)
        {
            *out = (Memmy_Value){.type = Memmy_Type_Range, .range = range};
        }
        return status;
    }
    if (expr->kind == MemmyAst_NodeKind_Index)
    {
        Memmy_Value list = {0};
        Memmy_Value index_value = {0};
        Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &list, error);
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &index_value, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (!Memmy_Type_IsList(list.type))
        {
            MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("index"),
                                String8_Lit("expected list value"));
            return Memmy_Status_InvalidArgument;
        }
        I64 index = 0;
        status = MemmyEval_Value_AsI64(&index_value, &index, error);
        if (status != Memmy_Status_Ok || index < 0 || (U64)index >= list.list.count)
        {
            MemmyEval_Error_Set(error, Memmy_Status_NotFound, String8_Lit("index"),
                                String8_Lit("list index out of range"));
            return Memmy_Status_NotFound;
        }
        *out = MemmyEval_ListItem(list, (U64)index);
        return Memmy_Status_Ok;
    }
    MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
