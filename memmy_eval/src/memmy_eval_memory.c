#include "memmy_eval_internal.h"

#include "base.h"

static Memmy_Status MemmyEval_ReadExact(Memmy_Process *process, Memmy_Addr address, U8 *bytes, U64 size,
                                        Memmy_Error *error)
{
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Process_Read(process, address, bytes, size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != size)
    {
        MemmyEval_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("read"), String8_Lit("partial typed read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyEval_String_Read(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                          Memmy_Value *out, Memmy_Error *error)
{
    U64 unit_size = type.string.encoding == Memmy_StringEncoding_Utf8 ? 1 : 2;
    U64 max_size =
        type.string.encoding == Memmy_StringEncoding_Utf8 ? MEMMY_EVAL_STRING_READ_MAX : MEMMY_EVAL_STRING_READ_MAX * 2;
    U8 *bytes = Arena_PushArrayNoZero(arena, U8, max_size + unit_size);
    U64 len = 0;
    while (len < max_size)
    {
        Memmy_Status status = MemmyEval_ReadExact(process, address + len, bytes + len, unit_size, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        len += unit_size;
        B32 terminated = 1;
        for (U64 i = 0; i < unit_size; i++)
        {
            terminated = terminated && bytes[len - unit_size + i] == 0;
        }
        if (terminated)
        {
            return Memmy_Value_Decode(arena, type, String8_Make(bytes, len), out, error);
        }
    }
    MemmyEval_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("read"),
                        String8_Lit("unterminated target string"));
    return Memmy_Status_InvalidEncoding;
}

Memmy_Status MemmyEval_Memory_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                        Memmy_Value *out, Memmy_Error *error)
{
    if (Memmy_Type_IsString(type))
    {
        return MemmyEval_String_Read(arena, process, address, type, out, error);
    }
    U64 size = Memmy_Type_EncodedSize(type);
    if (size == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("type"),
                            String8_Lit("type cannot be read from target memory"));
        return Memmy_Status_InvalidArgument;
    }
    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    Memmy_Status status = MemmyEval_ReadExact(process, address, bytes, size, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return Memmy_Value_Decode(arena, type, String8_Make(bytes, size), out, error);
}

Memmy_Status MemmyEval_Pointer_Read(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out, Memmy_Error *error)
{
    if (process == 0 || !Memmy_Process_IsOpen(process) || out == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                            String8_Lit("missing selected process for pointer dereference"));
        return Memmy_Status_InvalidArgument;
    }
    U64 size = process->pointer_width == Memmy_PointerWidth_32   ? 4
               : process->pointer_width == Memmy_PointerWidth_64 ? 8
                                                                 : 0;
    if (size == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("address"),
                            String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }
    U8 bytes[8] = {0};
    Memmy_Status status = MemmyEval_ReadExact(process, address, bytes, size, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    Memmy_Addr result = 0;
    for (U64 i = 0; i < size; i++)
    {
        result |= (Memmy_Addr)bytes[i] << (i * 8);
    }
    *out = result;
    return Memmy_Status_Ok;
}

Memmy_Status MemmyEval_Expr_EvalMemory(MemmyEval_Exec *exec, MemmyAst_Node const *expr, Memmy_Value *out,
                                       Memmy_Error *error)
{
    Memmy_Value base = {0};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(exec, expr->lhs, &base, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (expr->kind == MemmyAst_NodeKind_Deref)
    {
        Memmy_Addr address = 0;
        status = MemmyEval_Address_FromValue(&base, &address, error);
        Memmy_Process *process = 0;
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Process_Require(exec, String8_Lit("address"), &process, error);
        }
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Pointer_Read(process, address, &address, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->rhs != 0)
        {
            Memmy_Value offset = {0};
            status = MemmyEval_Expr_EvalWithContext(exec, expr->rhs, &offset, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            Memmy_Value address_value = {.type = Memmy_Type_Address, .address = address};
            return MemmyEval_Binary_Apply(MemmyAst_ConstOp_Add, address_value, offset, out, error);
        }
        *out = (Memmy_Value){.type = Memmy_Type_Address, .address = address};
        return Memmy_Status_Ok;
    }

    if (expr->kind == MemmyAst_NodeKind_TypedRead)
    {
        Memmy_Type type = {0};
        status = Memmy_Type_Parse(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (!Memmy_Type_IsAddress(base.type) && !Memmy_Type_IsRange(base.type))
        {
            return Memmy_Value_Convert(exec->out_arena, &base, type, out, error);
        }
        Memmy_Addr address = 0;
        status = MemmyEval_Address_FromValue(&base, &address, error);
        Memmy_Process *process = 0;
        if (status == Memmy_Status_Ok)
        {
            status = MemmyEval_Process_Require(exec, String8_Lit("read"), &process, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return MemmyEval_Memory_ReadValue(exec->out_arena, process, address, type, out, error);
    }

    MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
