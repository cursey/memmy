#include "memmy_exec.h"

#define MEMMY_EXEC_STRING_PEEK_MAX 4096
#define MEMMY_EXEC_STRING_PEEK_CHUNK_SIZE 256

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

static B32 Memmy_Exec_IsPrintableCodepoint(U32 cp)
{
    return (cp < 0x80 && Char8_IsPrint((U8)cp)) || (cp >= 0x80 && cp <= 0x10ffff);
}

typedef struct Memmy_ExecByteReader Memmy_ExecByteReader;
struct Memmy_ExecByteReader
{
    Memmy_Process *process;
    Memmy_Addr address;
    U64 offset;
    U8 buffer[MEMMY_EXEC_STRING_PEEK_CHUNK_SIZE];
    U64 pos;
    U64 count;
    Memmy_Status terminal_status;
};

static Memmy_Status Memmy_ExecByteReader_Read(Memmy_ExecByteReader *reader, U8 *out, Memmy_Error *error)
{
    if (reader->pos == reader->count)
    {
        if (reader->terminal_status != Memmy_Status_Ok)
        {
            return reader->terminal_status;
        }

        U64 bytes_read = 0;
        Memmy_Status status = Memmy_Process_Read(reader->process, reader->address + reader->offset, reader->buffer,
                                                 sizeof(reader->buffer), &bytes_read, error);
        if (status != Memmy_Status_Ok && bytes_read == 0)
        {
            return status;
        }
        if (status != Memmy_Status_Ok || bytes_read != sizeof(reader->buffer))
        {
            reader->terminal_status = status != Memmy_Status_Ok ? status : Memmy_Status_PartialRead;
        }
        reader->offset += bytes_read;
        reader->pos = 0;
        reader->count = bytes_read;
    }

    *out = reader->buffer[reader->pos++];
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ReadStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                       Memmy_Value *out, Memmy_Error *error)
{
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, MEMMY_EXEC_STRING_PEEK_MAX);
    U64 len = 0;
    Memmy_ExecByteReader reader = {
        .process = process,
        .address = address,
    };

    while (len < MEMMY_EXEC_STRING_PEEK_MAX)
    {
        U8 sequence[4];
        U64 need = 0;
        U32 cp = 0;
        Memmy_Status status = Memmy_ExecByteReader_Read(&reader, &sequence[0], error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        U8 first = sequence[0];
        if (first == 0)
        {
            break;
        }
        if (first < 0x80)
        {
            cp = first;
            need = 1;
        }
        else if (first >= 0xc2 && first <= 0xdf)
        {
            cp = first & 0x1f;
            need = 2;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            cp = first & 0x0f;
            need = 3;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            cp = first & 0x07;
            need = 4;
        }
        else
        {
            break;
        }
        if (len + need > MEMMY_EXEC_STRING_PEEK_MAX)
        {
            break;
        }

        B32 valid = 1;
        for (U64 i = 1; i < need; i++)
        {
            status = Memmy_ExecByteReader_Read(&reader, &sequence[i], error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if ((sequence[i] & 0xc0) != 0x80)
            {
                valid = 0;
                break;
            }
            cp = (cp << 6) | (sequence[i] & 0x3f);
        }
        if (!valid || (need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) ||
            !Memmy_Exec_IsPrintableCodepoint(cp))
        {
            break;
        }

        for (U64 i = 0; i < need; i++)
        {
            buffer[len++] = sequence[i];
        }
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(buffer, len)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ReadWStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                        Memmy_Value *out, Memmy_Error *error)
{
    U64 max_size = MEMMY_EXEC_STRING_PEEK_MAX * 2;
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, max_size);
    U64 len = 0;
    U64 offset = 0;
    U16 pending_high = 0;

    while (offset < max_size)
    {
        U8 chunk[MEMMY_EXEC_STRING_PEEK_CHUNK_SIZE];
        U64 remaining = max_size - offset;
        U64 size = Min(remaining, (U64)sizeof(chunk));
        if ((size & 1) != 0)
        {
            size--;
        }

        U64 bytes_read = 0;
        Memmy_Status status = Memmy_Process_Read(process, address + offset, chunk, size, &bytes_read, error);
        if (status != Memmy_Status_Ok && bytes_read == 0)
        {
            return status;
        }
        bytes_read &= ~1ull;
        offset += bytes_read;

        B32 stopped = 0;
        for (U64 i = 0; i < bytes_read; i += 2)
        {
            U16 unit = (U16)(chunk[i] | (chunk[i + 1] << 8));
            if (pending_high != 0)
            {
                if (unit < 0xdc00 || unit > 0xdfff)
                {
                    stopped = 1;
                    break;
                }
                if (len + 4 > max_size)
                {
                    stopped = 1;
                    break;
                }
                buffer[len++] = (U8)pending_high;
                buffer[len++] = (U8)(pending_high >> 8);
                buffer[len++] = (U8)unit;
                buffer[len++] = (U8)(unit >> 8);
                pending_high = 0;
                continue;
            }

            if (unit == 0 || (unit >= 0xdc00 && unit <= 0xdfff))
            {
                stopped = 1;
                break;
            }
            if (unit >= 0xd800 && unit <= 0xdbff)
            {
                pending_high = unit;
                continue;
            }
            if (!Memmy_Exec_IsPrintableCodepoint(unit))
            {
                stopped = 1;
                break;
            }
            buffer[len++] = (U8)unit;
            buffer[len++] = (U8)(unit >> 8);
        }

        if (stopped || len == max_size)
        {
            break;
        }
        if (status != Memmy_Status_Ok || bytes_read != size)
        {
            Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("peek"), String8_Lit("partial string read"));
            if (error != 0)
            {
                error->byte_count = bytes_read;
            }
            return Memmy_Status_PartialRead;
        }
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(buffer, len)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Exec_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                         Memmy_Value *out, Memmy_Error *error)
{
    if (type.kind == Memmy_TypeKind_Str)
    {
        return Memmy_Exec_ReadStr(arena, process, address, type, out, error);
    }
    if (type.kind == Memmy_TypeKind_WStr)
    {
        return Memmy_Exec_ReadWStr(arena, process, address, type, out, error);
    }

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

static B32 Memmy_Exec_TypeAcceptsConstValue(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_U8 || type.kind == Memmy_TypeKind_I8 || type.kind == Memmy_TypeKind_U16 ||
           type.kind == Memmy_TypeKind_I16 || type.kind == Memmy_TypeKind_U32 || type.kind == Memmy_TypeKind_I32 ||
           type.kind == Memmy_TypeKind_U64 || type.kind == Memmy_TypeKind_I64 || type.kind == Memmy_TypeKind_Ptr;
}

Memmy_Status Memmy_Exec_ValueParseWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process, Memmy_Type type,
                                          String8 text, Memmy_Value *out, Memmy_Error *error)
{
    String8 value_text = text;
    if (env != 0 && Memmy_Exec_TypeAcceptsConstValue(type) && String8_FindChar(text, '$', 0) != STRING8_NPOS)
    {
        Scratch scratch = Scratch_Begin(&arena, 1);
        Memmy_ConstExpr constant = {0};
        Memmy_Status status = Memmy_ConstExpr_Parse(scratch.arena, text, &constant, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }

        I64 value = 0;
        status = Memmy_ConstExpr_Resolve(env, process, &constant, &value, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }

        value_text = String8_PushF(scratch.arena, "%lld", value);
        status = Memmy_Value_Parse(arena, type, process->pointer_width, value_text, out, error);
        Scratch_End(scratch);
        return status;
    }

    return Memmy_Value_Parse(arena, type, process->pointer_width, value_text, out, error);
}

Memmy_Status Memmy_MemoryExpr_ExecutePeekWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
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
    status = Memmy_AddressExpr_ResolveWithEnv(env, process, &expr->address, &address, error);
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
        .pointer_width = process->pointer_width,
        .type = expr->type,
        .value = value,
    };
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_MemoryExpr_ExecutePeek(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPeekResult *out, Memmy_Error *error)
{
    return Memmy_MemoryExpr_ExecutePeekWithEnv(arena, 0, process, expr, out, error);
}

Memmy_Status Memmy_MemoryExpr_ExecutePokeWithEnv(Arena *arena, Memmy_ExecEnv *env, Memmy_Process *process,
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
    status = Memmy_AddressExpr_ResolveWithEnv(env, process, &expr->address, &address, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value new_value = {0};
    status = Memmy_Exec_ValueParseWithEnv(arena, env, process, expr->type, expr->value_text, &new_value, error);
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
        .pid = process->pid,
        .address = address,
        .pointer_width = process->pointer_width,
        .type = expr->type,
        .old_value = {.type = expr->type, .bytes = String8_Make(old_bytes, size)},
        .new_value = new_value,
    };
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_MemoryExpr_ExecutePoke(Arena *arena, Memmy_Process *process, Memmy_MemoryExpr *expr,
                                          Memmy_ExecPokeResult *out, Memmy_Error *error)
{
    return Memmy_MemoryExpr_ExecutePokeWithEnv(arena, 0, process, expr, out, error);
}
