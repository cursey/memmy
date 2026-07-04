#include "memmy_exec.h"

static Memmy_Status Memmy_Exec_RequireProcess(Memmy_Process *process, String8 context, Memmy_Error *error)
{
    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, context, String8_Lit("missing selected process"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_TypeSize(Memmy_Process *process, Memmy_Type type, U64 *out, Memmy_Error *error)
{
    if (type.kind == Memmy_TypeKind_Ptr)
    {
        if (process->pointer_width == Memmy_PointerWidth_32)
        {
            *out = 4;
            return Memmy_Status_Ok;
        }
        if (process->pointer_width == Memmy_PointerWidth_64)
        {
            *out = 8;
            return Memmy_Status_Ok;
        }
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }
    if (type.fixed_size == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("variable-width expression peeks are not supported"));
        return Memmy_Status_Unsupported;
    }

    *out = type.fixed_size;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                         Memmy_Value *out, Memmy_Error *error)
{
    U64 size = 0;
    Memmy_Status status = Memmy_Exec_TypeSize(process, type, &size, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U8 *buffer = Arena_PushArrayNoZero(arena, U8, size);
    U64 bytes_read = 0;
    status = Memmy_Process_Read(process, address, buffer, size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("peek"), String8_Lit("partial read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(buffer, size)};
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_MemoryExpr_ExecutePeek(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                          Memmy_MemoryExpr *expr, Memmy_ExecPeekResult *out, Memmy_Error *error)
{
    if (arena == 0 || expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("peek"),
                        String8_Lit("missing arena, expression, or output"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_Peek)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("peek"),
                        String8_Lit("memory expression is not a peek expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Status status = Memmy_Exec_RequireProcess(process, String8_Lit("peek"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Addr address = 0;
    status = Memmy_AddressExpr_Resolve(process, modules, &expr->address, &address, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value value = {0};
    status = Memmy_Exec_ReadValue(arena, process, address, expr->type, &value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out = (Memmy_ExecPeekResult){
        .address = address,
        .type = expr->type,
        .value = value,
    };
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_MemoryExpr_ExecutePoke(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                          Memmy_MemoryExpr *expr, Memmy_ExecPokeResult *out, Memmy_Error *error)
{
    if (arena == 0 || expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("poke"),
                        String8_Lit("missing arena, expression, or output"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_Poke)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("poke"),
                        String8_Lit("memory expression is not a poke expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Status status = Memmy_Exec_RequireProcess(process, String8_Lit("poke"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Addr address = 0;
    status = Memmy_AddressExpr_Resolve(process, modules, &expr->address, &address, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value new_value = {0};
    status = Memmy_Value_Parse(arena, expr->type, process->pointer_width, expr->value_text, &new_value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U64 size = new_value.bytes.len;
    U8 *old_bytes = Arena_PushArrayNoZero(arena, U8, size);
    U64 byte_count = 0;
    status = Memmy_Process_Read(process, address, old_bytes, size, &byte_count, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (byte_count != size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("poke"), String8_Lit("partial old-value read"));
        if (error != 0)
        {
            error->byte_count = byte_count;
        }
        return Memmy_Status_PartialRead;
    }

    byte_count = 0;
    status = Memmy_Process_Write(process, address, new_value.bytes.data, new_value.bytes.len, &byte_count, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (byte_count != new_value.bytes.len)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialWrite, String8_Lit("poke"), String8_Lit("partial write"));
        if (error != 0)
        {
            error->byte_count = byte_count;
        }
        return Memmy_Status_PartialWrite;
    }

    *out = (Memmy_ExecPokeResult){
        .address = address,
        .type = expr->type,
        .old_value = {.type = expr->type, .bytes = String8_Make(old_bytes, size)},
        .new_value = new_value,
    };
    return Memmy_Status_Ok;
}
