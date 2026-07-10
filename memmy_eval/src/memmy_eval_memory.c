#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

Memmy_Status Memmy_Eval_ParseType(String8 type_name, Memmy_Type *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Type_Parse(type_name, out, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_TypeSize(Memmy_Process *process, Memmy_Type type, U64 *out, Memmy_Error *error)
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
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }
    if (type.fixed_size == 0)
    {
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("variable-width typed reads are not supported"));
        return Memmy_Status_Unsupported;
    }

    *out = type.fixed_size;
    return Memmy_Status_Ok;
}

static B32 Memmy_Eval_IsPrintableCodepoint(U32 cp)
{
    return (cp < 0x80 && Char8_IsPrint((U8)cp)) || (cp >= 0x80 && cp <= 0x10ffff);
}

static Memmy_Status Memmy_EvalByteReader_Read(Memmy_EvalByteReader *reader, U8 *out, Memmy_Error *error)
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

static Memmy_Status Memmy_Eval_ReadStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                       Memmy_Value *out, Memmy_Error *error)
{
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, MEMMY_EVAL_STRING_READ_MAX);
    U64 len = 0;
    Memmy_EvalByteReader reader = {
        .process = process,
        .address = address,
    };

    while (len < MEMMY_EVAL_STRING_READ_MAX)
    {
        U8 sequence[4];
        U64 need = 0;
        U32 cp = 0;
        Memmy_Status status = Memmy_EvalByteReader_Read(&reader, &sequence[0], error);
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
        if (len + need > MEMMY_EVAL_STRING_READ_MAX)
        {
            break;
        }

        B32 valid = 1;
        for (U64 i = 1; i < need; i++)
        {
            status = Memmy_EvalByteReader_Read(&reader, &sequence[i], error);
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
            !Memmy_Eval_IsPrintableCodepoint(cp))
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

static Memmy_Status Memmy_Eval_ReadWStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                        Memmy_Value *out, Memmy_Error *error)
{
    U64 max_size = MEMMY_EVAL_STRING_READ_MAX * 2;
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, max_size);
    U64 len = 0;
    U64 offset = 0;
    U16 pending_high = 0;

    while (offset < max_size)
    {
        U8 chunk[MEMMY_EVAL_STRING_READ_CHUNK_SIZE];
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
            if (!Memmy_Eval_IsPrintableCodepoint(unit))
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
            Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("read"), String8_Lit("partial string read"));
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

I64 Memmy_Eval_IntegerFromBytes(Memmy_Value value)
{
    U64 raw = 0;
    U64 size = Min(value.bytes.len, (U64)sizeof(raw));
    for (U64 i = 0; i < size; i++)
    {
        raw |= ((U64)value.bytes.data[i]) << (i * 8);
    }

    switch (value.type.kind)
    {
    case Memmy_TypeKind_I8:
        return (I8)raw;
    case Memmy_TypeKind_I16:
        return (I16)raw;
    case Memmy_TypeKind_I32:
        return (I32)raw;
    case Memmy_TypeKind_I64:
        return (I64)raw;
    default:
        return (I64)raw;
    }
}

Memmy_Status Memmy_Eval_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                  Memmy_Value *out, Memmy_Error *error)
{
    if (type.kind == Memmy_TypeKind_Str)
    {
        return Memmy_Eval_ReadStr(arena, process, address, type, out, error);
    }
    if (type.kind == Memmy_TypeKind_WStr)
    {
        return Memmy_Eval_ReadWStr(arena, process, address, type, out, error);
    }

    U64 size = 0;
    Memmy_Status status = Memmy_Eval_TypeSize(process, type, &size, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    U64 bytes_read = 0;
    status = Memmy_Process_Read(process, address, bytes, size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != size)
    {
        Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("read"), String8_Lit("partial typed read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, size)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_DecodeStringLiteral(Arena *arena, String8 text, String8 *out, Memmy_Error *error)
{
    if (text.len < 2 || text.data[0] != '"' || text.data[text.len - 1] != '"')
    {
        *out = text;
        return Memmy_Status_Ok;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, text.len - 2);
    U64 at = 0;
    for (U64 i = 1; i + 1 < text.len; i++)
    {
        U8 c = text.data[i];
        if (c == '\\')
        {
            i++;
            if (i + 1 >= text.len)
            {
                Memmy_EvalError(error, Memmy_Status_ParseError, String8_Lit("value"),
                                String8_Lit("unterminated string escape"));
                return Memmy_Status_ParseError;
            }

            U8 esc = text.data[i];
            if (esc == '"')
            {
                c = '"';
            }
            else if (esc == '\\')
            {
                c = '\\';
            }
            else if (esc == 'n')
            {
                c = '\n';
            }
            else if (esc == 'r')
            {
                c = '\r';
            }
            else if (esc == 't')
            {
                c = '\t';
            }
            else
            {
                Memmy_EvalError(error, Memmy_Status_ParseError, String8_Lit("value"),
                                String8_Lit("unknown string escape"));
                return Memmy_Status_ParseError;
            }
        }

        bytes[at++] = c;
    }

    *out = String8_Make(bytes, at);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Eval_ParseValue(Memmy_EvalExec *exec, Memmy_Process *process, Memmy_Type type, String8 value_text,
                                   Memmy_Value *out, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    String8 parse_text = value_text;
    Scratch scratch = {0};
    B32 using_scratch = 0;
    if (Memmy_EvalValue_IsIntegerTyped(
            &(Memmy_EvalValue){.kind = Memmy_EvalValueKind_TypedValue, .typed_value = {.type = type}}) &&
        String8_FindChar(value_text, '$', 0) != STRING8_NPOS)
    {
        scratch = Scratch_Begin((Arena *[]){env->arena, exec->out_arena}, 2);
        using_scratch = 1;
        Memmy_AstNode *expr = 0;
        Memmy_AstDiagnostic diagnostic = {0};
        Memmy_AstStatus ast_status = Memmy_Ast_ParseExpr(scratch.arena, value_text, &expr, &diagnostic);
        if (ast_status != Memmy_AstStatus_Ok)
        {
            Scratch_End(scratch);
            Memmy_EvalError(error, Memmy_Status_ParseError, String8_Lit("value"),
                            String8_Lit("invalid value expression"));
            return Memmy_Status_ParseError;
        }

        Memmy_EvalValue value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr, &value, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        I64 constant = 0;
        status = Memmy_EvalValue_AsConst(&value, &constant, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        parse_text = String8_PushF(scratch.arena, "%lld", constant);
    }

    if (type.kind == Memmy_TypeKind_Str || type.kind == Memmy_TypeKind_WStr)
    {
        if (!using_scratch)
        {
            scratch = Scratch_Begin((Arena *[]){env->arena, exec->out_arena}, 2);
            using_scratch = 1;
        }
        Memmy_Status decode_status = Memmy_Eval_DecodeStringLiteral(scratch.arena, parse_text, &parse_text, error);
        if (decode_status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return decode_status;
        }
    }

    Memmy_Status status = Memmy_Value_Parse(exec->out_arena, type, process->pointer_width, parse_text, out, error);
    if (using_scratch)
    {
        Scratch_End(scratch);
    }
    return status;
}

Memmy_Status Memmy_Eval_ReadPointer(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out, Memmy_Error *error)
{
    if (process == 0 || !Memmy_Process_IsOpen(process))
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("missing selected process for pointer dereference"));
        return Memmy_Status_InvalidArgument;
    }

    U64 pointer_size = 0;
    if (process->pointer_width == Memmy_PointerWidth_32)
    {
        pointer_size = 4;
    }
    else if (process->pointer_width == Memmy_PointerWidth_64)
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
    Memmy_Status status = Memmy_Process_Read(process, address, bytes, pointer_size, &bytes_read, error);
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

Memmy_Status Memmy_Eval_MemoryExpr(Memmy_EvalExec *exec, Memmy_AstNode const *expr, Memmy_EvalValue *out,
                                   Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)env;
    if (expr->kind == Memmy_AstNodeKind_Deref)
    {
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
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("address"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Eval_ReadPointer(process, address, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->rhs != 0)
        {
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
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = address};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_TypedRead)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("read"), &process, error);
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

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value value = {0};
        status = Memmy_Eval_ReadValue(exec->out_arena, process, address, type, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_TypedValue,
            .constant = Memmy_Eval_IntegerFromBytes(value),
            .address = address,
            .typed_value = value,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_TypedWrite)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("write"), &process, error);
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

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value old_value = {0};
        status = Memmy_Eval_ReadValue(exec->out_arena, process, address, type, &old_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value new_value = {0};
        status = Memmy_Eval_ParseValue(exec, process, type, expr->value_text, &new_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        U64 bytes_written = 0;
        status =
            Memmy_Process_Write(process, address, new_value.bytes.data, new_value.bytes.len, &bytes_written, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (bytes_written != new_value.bytes.len)
        {
            Memmy_EvalError(error, Memmy_Status_PartialWrite, String8_Lit("write"), String8_Lit("partial typed write"));
            if (error != 0)
            {
                error->byte_count = bytes_written;
            }
            return Memmy_Status_PartialWrite;
        }

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_TypedValue,
            .constant = Memmy_Eval_IntegerFromBytes(new_value),
            .address = address,
            .typed_value = new_value,
            .old_typed_value = old_value,
        };
        return Memmy_Status_Ok;
    }
    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}
