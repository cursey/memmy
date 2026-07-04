#include "memmy_cli_internal.h"

#include <math.h>
#include <string.h>

static U64 Memmy_Cli_ReadLE(String8 bytes)
{
    U64 result = 0;
    U64 count = Min(bytes.len, 8);
    for (U64 i = 0; i < count; i++)
    {
        result |= ((U64)bytes.data[i]) << (i * 8);
    }
    return result;
}

static I64 Memmy_Cli_ReadSLE(String8 bytes)
{
    U64 unsigned_value = Memmy_Cli_ReadLE(bytes);
    if (bytes.len > 0 && bytes.len < 8 && (bytes.data[bytes.len - 1] & 0x80))
    {
        unsigned_value |= U64_MAX << (bytes.len * 8);
    }
    return (I64)unsigned_value;
}

static Memmy_Status Memmy_Cli_ValidateUtf8(String8 text, Memmy_Error *error)
{
    U64 i = 0;
    while (i < text.len)
    {
        U8 first = text.data[i];
        U32 codepoint = 0;
        U64 need = 0;
        if (first < 0x80)
        {
            codepoint = first;
            need = 1;
        }
        else if (first >= 0xc2 && first <= 0xdf)
        {
            codepoint = first & 0x1f;
            need = 2;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            codepoint = first & 0x0f;
            need = 3;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            codepoint = first & 0x07;
            need = 4;
        }
        else
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("invalid utf-8 leading byte"));
            return Memmy_Status_InvalidEncoding;
        }

        if (i + need > text.len)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("truncated utf-8 sequence"));
            return Memmy_Status_InvalidEncoding;
        }
        for (U64 j = 1; j < need; j++)
        {
            U8 c = text.data[i + j];
            if ((c & 0xc0) != 0x80)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("invalid utf-8 continuation byte"));
                return Memmy_Status_InvalidEncoding;
            }
            codepoint = (codepoint << 6) | (c & 0x3f);
        }
        if ((need == 3 && codepoint < 0x800) || (need == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("invalid utf-8 codepoint"));
            return Memmy_Status_InvalidEncoding;
        }
        i += need;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ValidateWStr(String8 bytes, Memmy_Error *error)
{
    if ((bytes.len & 1) != 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                        String8_Lit("truncated utf-16 code unit"));
        return Memmy_Status_InvalidEncoding;
    }

    for (U64 i = 0; i < bytes.len; i += 2)
    {
        U16 unit = (U16)(bytes.data[i] | (bytes.data[i + 1] << 8));
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            if (i + 3 >= bytes.len)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("truncated utf-16 surrogate pair"));
                return Memmy_Status_InvalidEncoding;
            }
            U16 low = (U16)(bytes.data[i + 2] | (bytes.data[i + 3] << 8));
            if (low < 0xdc00 || low > 0xdfff)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("invalid utf-16 surrogate pair"));
                return Memmy_Status_InvalidEncoding;
            }
            i += 2;
        }
        else if (unit >= 0xdc00 && unit <= 0xdfff)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("unpaired utf-16 low surrogate"));
            return Memmy_Status_InvalidEncoding;
        }
    }
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_DecodeWStr(Arena *arena, String8 bytes)
{
    U8 *out = Arena_PushArrayNoZero(arena, U8, bytes.len * 2 + 1);
    U64 at = 0;
    for (U64 i = 0; i < bytes.len; i += 2)
    {
        U32 cp = (U32)(bytes.data[i] | (bytes.data[i + 1] << 8));
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            U32 low = (U32)(bytes.data[i + 2] | (bytes.data[i + 3] << 8));
            cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
            i += 2;
        }

        if (cp < 0x80)
        {
            out[at++] = (U8)cp;
        }
        else if (cp < 0x800)
        {
            out[at++] = (U8)(0xc0 | (cp >> 6));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
        else if (cp < 0x10000)
        {
            out[at++] = (U8)(0xe0 | (cp >> 12));
            out[at++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
        else
        {
            out[at++] = (U8)(0xf0 | (cp >> 18));
            out[at++] = (U8)(0x80 | ((cp >> 12) & 0x3f));
            out[at++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
    }
    out[at] = 0;
    return String8_Make(out, at);
}

static String8 Memmy_Cli_EscapeString(Arena *arena, String8 text)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("\""));
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        switch (c)
        {
        case 0:
            String8List_Push(arena, &parts, String8_Lit("\\0"));
            break;
        case '\n':
            String8List_Push(arena, &parts, String8_Lit("\\n"));
            break;
        case '\r':
            String8List_Push(arena, &parts, String8_Lit("\\r"));
            break;
        case '\t':
            String8List_Push(arena, &parts, String8_Lit("\\t"));
            break;
        case '\\':
            String8List_Push(arena, &parts, String8_Lit("\\\\"));
            break;
        case '"':
            String8List_Push(arena, &parts, String8_Lit("\\\""));
            break;
        default:
            if (c < 0x20 || c == 0x7f)
            {
                String8List_Push(arena, &parts, String8_PushF(arena, "\\x%02x", c));
            }
            else
            {
                String8List_Push(arena, &parts, String8_Make(text.data + i, 1));
            }
            break;
        }
    }
    String8List_Push(arena, &parts, String8_Lit("\""));
    return String8List_Join(arena, &parts, (String8){0});
}

static void Memmy_Cli_PushHexBytes(Arena *arena, String8List *lines, String8 bytes)
{
    for (U64 i = 0; i < bytes.len; i++)
    {
        Memmy_Cli_PushLine(arena, lines, "%s%02x", i == 0 ? "" : " ", bytes.data[i]);
    }
}

static Memmy_Status Memmy_Cli_ResolveReadSize(Memmy_Process *process, Memmy_CliOptions *options, U64 *out_size,
                                              Memmy_Error *error)
{
    Memmy_Type type = options->type;
    if (type.kind == Memmy_TypeKind_Ptr)
    {
        if (options->has_count)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("ptr rejects --count"));
            return Memmy_Status_ParseError;
        }
        if (process->pointer_width == Memmy_PointerWidth_32)
        {
            *out_size = 4;
            return Memmy_Status_Ok;
        }
        if (process->pointer_width == Memmy_PointerWidth_64)
        {
            *out_size = 8;
            return Memmy_Status_Ok;
        }
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("peek"),
                        String8_Lit("unknown process pointer width"));
        return Memmy_Status_InvalidArgument;
    }
    if (type.kind == Memmy_TypeKind_Bytes || type.kind == Memmy_TypeKind_Str || type.kind == Memmy_TypeKind_WStr)
    {
        if (!options->has_count)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"),
                            String8_Lit("variable-width type requires --count"));
            return Memmy_Status_ParseError;
        }
        if (type.kind == Memmy_TypeKind_WStr)
        {
            if (options->count > U64_MAX / 2)
            {
                Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("peek"), String8_Lit("wstr count overflow"));
                return Memmy_Status_Overflow;
            }
            *out_size = options->count * 2;
        }
        else
        {
            *out_size = options->count;
        }
        return Memmy_Status_Ok;
    }
    if (options->has_count)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"),
                        String8_Lit("fixed-width type rejects --count"));
        return Memmy_Status_ParseError;
    }
    *out_size = type.fixed_size;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_FormatPeekValue(Arena *arena, Memmy_CliOptions *options, String8 bytes,
                                              String8List *lines, Memmy_Error *error)
{
    Memmy_Type type = options->type;
    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
    case Memmy_TypeKind_U16:
    case Memmy_TypeKind_U32:
    case Memmy_TypeKind_U64:
    case Memmy_TypeKind_Ptr: {
        U64 value = Memmy_Cli_ReadLE(bytes);
        Memmy_Cli_PushLine(arena, lines, "%llu  0x%0*llx", (unsigned long long)value, (int)(bytes.len * 2),
                           (unsigned long long)value);
    }
    break;
    case Memmy_TypeKind_I8:
    case Memmy_TypeKind_I16:
    case Memmy_TypeKind_I32:
    case Memmy_TypeKind_I64: {
        I64 value = Memmy_Cli_ReadSLE(bytes);
        U64 hex = Memmy_Cli_ReadLE(bytes);
        Memmy_Cli_PushLine(arena, lines, "%lld  0x%0*llx", (long long)value, (int)(bytes.len * 2),
                           (unsigned long long)hex);
    }
    break;
    case Memmy_TypeKind_F32: {
        F32 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        Memmy_Cli_PushLine(arena, lines, "%g", (double)value);
    }
    break;
    case Memmy_TypeKind_F64: {
        F64 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        Memmy_Cli_PushLine(arena, lines, "%g", value);
    }
    break;
    case Memmy_TypeKind_Bytes:
        Memmy_Cli_PushHexBytes(arena, lines, bytes);
        break;
    case Memmy_TypeKind_Str: {
        Memmy_Status status = Memmy_Cli_ValidateUtf8(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 escaped = Memmy_Cli_EscapeString(arena, bytes);
        String8List_Push(arena, lines, escaped);
    }
    break;
    case Memmy_TypeKind_WStr: {
        Memmy_Status status = Memmy_Cli_ValidateWStr(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = Memmy_Cli_DecodeWStr(arena, bytes);
        String8 escaped = Memmy_Cli_EscapeString(arena, text);
        String8List_Push(arena, lines, escaped);
    }
    break;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_FormatJsonValueFields(Arena *arena, Memmy_CliOptions *options, String8 bytes,
                                                    String8 *out, Memmy_Error *error)
{
    Memmy_Type type = options->type;
    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
    case Memmy_TypeKind_U16:
    case Memmy_TypeKind_U32:
    case Memmy_TypeKind_U64:
    case Memmy_TypeKind_Ptr: {
        U64 value = Memmy_Cli_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%llu,\"hex\":\"0x%0*llx\"", (unsigned long long)value,
                             (int)(bytes.len * 2), (unsigned long long)value);
    }
    break;
    case Memmy_TypeKind_I8:
    case Memmy_TypeKind_I16:
    case Memmy_TypeKind_I32:
    case Memmy_TypeKind_I64: {
        I64 value = Memmy_Cli_ReadSLE(bytes);
        U64 hex = Memmy_Cli_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%lld,\"hex\":\"0x%0*llx\"", (long long)value, (int)(bytes.len * 2),
                             (unsigned long long)hex);
    }
    break;
    case Memmy_TypeKind_F32: {
        F32 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        if (isfinite((double)value))
        {
            *out = String8_PushF(arena, "\"value\":%g", (double)value);
        }
        else
        {
            *out = String8_Lit("\"value\":null");
        }
    }
    break;
    case Memmy_TypeKind_F64: {
        F64 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        if (isfinite(value))
        {
            *out = String8_PushF(arena, "\"value\":%g", value);
        }
        else
        {
            *out = String8_Lit("\"value\":null");
        }
    }
    break;
    case Memmy_TypeKind_Bytes: {
        String8 hex_bytes = Memmy_Cli_FormatHexBytes(arena, bytes);
        String8 value = Memmy_Cli_FormatJsonString(arena, hex_bytes);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    case Memmy_TypeKind_Str: {
        Memmy_Status status = Memmy_Cli_ValidateUtf8(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 value = Memmy_Cli_FormatJsonString(arena, bytes);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    case Memmy_TypeKind_WStr: {
        Memmy_Status status = Memmy_Cli_ValidateWStr(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = Memmy_Cli_DecodeWStr(arena, bytes);
        String8 value = Memmy_Cli_FormatJsonString(arena, text);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    }
    return Memmy_Status_Ok;
}

String8 Memmy_Cli_TypeString(Memmy_Type type)
{
    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
        return String8_Lit("u8");
    case Memmy_TypeKind_I8:
        return String8_Lit("i8");
    case Memmy_TypeKind_U16:
        return String8_Lit("u16");
    case Memmy_TypeKind_I16:
        return String8_Lit("i16");
    case Memmy_TypeKind_U32:
        return String8_Lit("u32");
    case Memmy_TypeKind_I32:
        return String8_Lit("i32");
    case Memmy_TypeKind_U64:
        return String8_Lit("u64");
    case Memmy_TypeKind_I64:
        return String8_Lit("i64");
    case Memmy_TypeKind_F32:
        return String8_Lit("f32");
    case Memmy_TypeKind_F64:
        return String8_Lit("f64");
    case Memmy_TypeKind_Ptr:
        return String8_Lit("ptr");
    case Memmy_TypeKind_Bytes:
        return String8_Lit("bytes");
    case Memmy_TypeKind_Str:
        return String8_Lit("str");
    case Memmy_TypeKind_WStr:
        return String8_Lit("wstr");
    }
    return String8_Lit("?");
}

Memmy_Status Memmy_Cli_FormatPeekOutput(Arena *arena, Memmy_CliPeekOutput *peek, B32 json, String8 *out,
                                        Memmy_Error *error)
{
    Memmy_CliOptions format_options = {
        .type = peek->type,
        .type_text = peek->type_text.len != 0 ? peek->type_text : Memmy_Cli_TypeString(peek->type),
    };
    String8 address = Memmy_Cli_FormatAddress(arena, peek->pointer_width, peek->address);
    if (json)
    {
        String8 value_fields = {0};
        Memmy_Status status =
            Memmy_Cli_FormatJsonValueFields(arena, &format_options, peek->bytes, &value_fields, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 type_json = Memmy_Cli_FormatJsonString(arena, format_options.type_text);
        *out =
            String8_PushF(arena, "{\"address\":\"%.*s\",\"type\":%.*s,%.*s}\n", (int)address.len, (char *)address.data,
                          (int)type_json.len, (char *)type_json.data, (int)value_fields.len, (char *)value_fields.data);
    }
    else
    {
        String8List lines = {0};
        Memmy_Cli_PushLine(arena, &lines, "%.*s: %.*s ", (int)address.len, (char *)address.data,
                           (int)format_options.type_text.len, (char *)format_options.type_text.data);
        Memmy_Status status = Memmy_Cli_FormatPeekValue(arena, &format_options, peek->bytes, &lines, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8List_Push(arena, &lines, String8_Lit("\n"));
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RunPeek(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_Read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("peek"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("peek"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_filter)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for peek"), String8_Lit("--filter"));
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!options->has_addr)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("peek requires --addr"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_type)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("peek requires --type"));
        return Memmy_Status_ParseError;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Read, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U64 read_size = 0;
    status = Memmy_Cli_ResolveReadSize(process, options, &read_size, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    U8 *buffer = Arena_PushArrayNoZero(arena, U8, read_size);
    U64 bytes_read = 0;
    Memmy_PointerWidth pointer_width = process->pointer_width;
    status = Memmy_Process_Read(process, options->addr, buffer, read_size, &bytes_read, error);
    Memmy_Process_Close(process);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != read_size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("peek"), String8_Lit("partial read"));
        return Memmy_Status_PartialRead;
    }

    Memmy_CliPeekOutput peek = {
        .pointer_width = pointer_width,
        .address = options->addr,
        .type = options->type,
        .type_text = options->type_text,
        .bytes = String8_Make(buffer, read_size),
    };
    return Memmy_Cli_FormatPeekOutput(arena, &peek, options->json, out, error);
}

Memmy_Status Memmy_Cli_FormatValue(Arena *arena, Memmy_CliOptions *options, String8 bytes, String8 *out,
                                   Memmy_Error *error)
{
    String8List parts = {0};
    Memmy_Status status = Memmy_Cli_FormatPeekValue(arena, options, bytes, &parts, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = String8List_Join(arena, &parts, (String8){0});
    return Memmy_Status_Ok;
}
