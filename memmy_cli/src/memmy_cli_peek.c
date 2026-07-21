#include "memmy_cli_internal.h"

#include "base.h"

static U64 MemmyCli_Integer_ReadLE(String8 bytes)
{
    U64 result = 0;
    U64 count = Min(bytes.len, 8);
    for (U64 i = 0; i < count; i++)
    {
        result |= ((U64)bytes.data[i]) << (i * 8);
    }
    return result;
}

static I64 MemmyCli_Integer_ReadSLE(String8 bytes)
{
    U64 unsigned_value = MemmyCli_Integer_ReadLE(bytes);
    if (bytes.len > 0 && bytes.len < 8 && (bytes.data[bytes.len - 1] & 0x80))
    {
        unsigned_value |= U64_MAX << (bytes.len * 8);
    }
    return (I64)unsigned_value;
}

static Memmy_Status MemmyCli_Utf8_Validate(String8 text, Memmy_Error *error)
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

static Memmy_Status MemmyCli_WString_Validate(String8 bytes, Memmy_Error *error)
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

static String8 MemmyCli_WString_Decode(Arena *arena, String8 bytes)
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

static String8 MemmyCli_String_Escape(Arena *arena, String8 text)
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

static void MemmyCli_HexBytes_Push(Arena *arena, String8List *lines, String8 bytes)
{
    for (U64 i = 0; i < bytes.len; i++)
    {
        MemmyCli_Line_Push(arena, lines, "%s%02x", i == 0 ? "" : " ", bytes.data[i]);
    }
}

static String8 MemmyCli_JsonHexBytes_Format(Arena *arena, String8 bytes)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("\"0x"));
    for (U64 i = 0; i < bytes.len; i++)
    {
        MemmyCli_Line_Push(arena, &parts, "%02x", bytes.data[i]);
    }
    String8List_Push(arena, &parts, String8_Lit("\""));
    return String8List_Join(arena, &parts, (String8){0});
}

static Memmy_Status MemmyCli_PeekValue_Format(Arena *arena, MemmyCli_ValueFormat *format, String8 bytes,
                                              String8List *lines, Memmy_Error *error)
{
    Memmy_Type type = format->type;
    if (Memmy_Type_IsInteger(type) && !type.integer.is_signed)
    {
        U64 value = MemmyCli_Integer_ReadLE(bytes);
        MemmyCli_Line_Push(arena, lines, "%llu  0x%0*llx", (unsigned long long)value, (int)(bytes.len * 2),
                           (unsigned long long)value);
    }
    else if (Memmy_Type_IsInteger(type))
    {
        I64 value = MemmyCli_Integer_ReadSLE(bytes);
        U64 hex = MemmyCli_Integer_ReadLE(bytes);
        MemmyCli_Line_Push(arena, lines, "%lld  0x%0*llx", (long long)value, (int)(bytes.len * 2),
                           (unsigned long long)hex);
    }
    else if (Memmy_Type_IsFloat(type) && type.floating.bit_count == 32)
    {
        F32 value = 0;
        Memory_Copy(&value, bytes.data, sizeof(value));
        MemmyCli_Line_Push(arena, lines, "%g", (double)value);
    }
    else if (Memmy_Type_IsFloat(type))
    {
        F64 value = 0;
        Memory_Copy(&value, bytes.data, sizeof(value));
        MemmyCli_Line_Push(arena, lines, "%g", value);
    }
    else if (Memmy_Type_IsString(type) && type.string.encoding == Memmy_StringEncoding_Utf8)
    {
        Memmy_Status status = MemmyCli_Utf8_Validate(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 escaped = MemmyCli_String_Escape(arena, bytes);
        String8List_Push(arena, lines, escaped);
    }
    else if (Memmy_Type_IsString(type))
    {
        Memmy_Status status = MemmyCli_WString_Validate(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = MemmyCli_WString_Decode(arena, bytes);
        String8 escaped = MemmyCli_String_Escape(arena, text);
        String8List_Push(arena, lines, escaped);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status MemmyCli_JsonValueFields_Format(Arena *arena, MemmyCli_ValueFormat *format, String8 bytes,
                                                    String8 *out, Memmy_Error *error)
{
    Memmy_Type type = format->type;
    if (Memmy_Type_IsInteger(type) && !type.integer.is_signed)
    {
        U64 value = MemmyCli_Integer_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%llu,\"hex\":\"0x%0*llx\"", (unsigned long long)value,
                             (int)(bytes.len * 2), (unsigned long long)value);
    }
    else if (Memmy_Type_IsInteger(type))
    {
        I64 value = MemmyCli_Integer_ReadSLE(bytes);
        U64 hex = MemmyCli_Integer_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%lld,\"hex\":\"0x%0*llx\"", (long long)value, (int)(bytes.len * 2),
                             (unsigned long long)hex);
    }
    else if (Memmy_Type_IsFloat(type) && type.floating.bit_count == 32)
    {
        F32 value = 0;
        Memory_Copy(&value, bytes.data, sizeof(value));
        String8 hex = MemmyCli_JsonHexBytes_Format(arena, bytes);
        if (F32_IsFinite(value))
        {
            *out = String8_PushF(arena, "\"value\":%g,\"hex\":%.*s", (double)value, (int)hex.len, (char *)hex.data);
        }
        else
        {
            *out = String8_PushF(arena, "\"value\":null,\"hex\":%.*s", (int)hex.len, (char *)hex.data);
        }
    }
    else if (Memmy_Type_IsFloat(type))
    {
        F64 value = 0;
        Memory_Copy(&value, bytes.data, sizeof(value));
        String8 hex = MemmyCli_JsonHexBytes_Format(arena, bytes);
        if (F64_IsFinite(value))
        {
            *out = String8_PushF(arena, "\"value\":%g,\"hex\":%.*s", value, (int)hex.len, (char *)hex.data);
        }
        else
        {
            *out = String8_PushF(arena, "\"value\":null,\"hex\":%.*s", (int)hex.len, (char *)hex.data);
        }
    }
    else if (Memmy_Type_IsString(type) && type.string.encoding == Memmy_StringEncoding_Utf8)
    {
        Memmy_Status status = MemmyCli_Utf8_Validate(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 value = MemmyCli_JsonString_Format(arena, bytes);
        String8 hex = MemmyCli_JsonHexBytes_Format(arena, bytes);
        *out = String8_PushF(arena, "\"value\":%.*s,\"hex\":%.*s", (int)value.len, (char *)value.data, (int)hex.len,
                             (char *)hex.data);
    }
    else if (Memmy_Type_IsString(type))
    {
        Memmy_Status status = MemmyCli_WString_Validate(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = MemmyCli_WString_Decode(arena, bytes);
        String8 value = MemmyCli_JsonString_Format(arena, text);
        String8 hex = MemmyCli_JsonHexBytes_Format(arena, bytes);
        *out = String8_PushF(arena, "\"value\":%.*s,\"hex\":%.*s", (int)value.len, (char *)value.data, (int)hex.len,
                             (char *)hex.data);
    }
    return Memmy_Status_Ok;
}

String8 MemmyCli_Type_String(Memmy_Type type)
{
    if (Memmy_Type_IsInteger(type))
    {
        if (type.integer.bit_count == 8)
        {
            return type.integer.is_signed ? String8_Lit("i8") : String8_Lit("u8");
        }
        if (type.integer.bit_count == 16)
        {
            return type.integer.is_signed ? String8_Lit("i16") : String8_Lit("u16");
        }
        if (type.integer.bit_count == 32)
        {
            return type.integer.is_signed ? String8_Lit("i32") : String8_Lit("u32");
        }
        return type.integer.is_signed ? String8_Lit("i64") : String8_Lit("u64");
    }
    if (Memmy_Type_IsFloat(type))
    {
        return type.floating.bit_count == 32 ? String8_Lit("f32") : String8_Lit("f64");
    }
    if (Memmy_Type_IsString(type))
    {
        return type.string.encoding == Memmy_StringEncoding_Utf8 ? String8_Lit("str") : String8_Lit("wstr");
    }
    return String8_Lit("?");
}

Memmy_Status MemmyCli_PeekOutput_Format(Arena *arena, MemmyCli_PeekOutput *peek, B32 jsonl, String8 *out,
                                        Memmy_Error *error)
{
    MemmyCli_ValueFormat format = {
        .type = peek->type,
        .type_text = peek->type_text.len != 0 ? peek->type_text : MemmyCli_Type_String(peek->type),
    };
    String8 address = MemmyCli_Address_Format(arena, peek->pointer_width, peek->address);
    if (jsonl)
    {
        String8 value_fields = {0};
        Memmy_Status status = MemmyCli_JsonValueFields_Format(arena, &format, peek->bytes, &value_fields, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 type_json = MemmyCli_JsonString_Format(arena, format.type_text);
        *out = String8_PushF(arena, "{\"type\":\"peek\",\"address\":\"%.*s\",\"value_type\":%.*s,%.*s}\n",
                             (int)address.len, (char *)address.data, (int)type_json.len, (char *)type_json.data,
                             (int)value_fields.len, (char *)value_fields.data);
    }
    else
    {
        String8List lines = {0};
        MemmyCli_Line_Push(arena, &lines, "%.*s: %.*s ", (int)address.len, (char *)address.data,
                           (int)format.type_text.len, (char *)format.type_text.data);
        Memmy_Status status = MemmyCli_PeekValue_Format(arena, &format, peek->bytes, &lines, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8List_Push(arena, &lines, String8_Lit("\n"));
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}

Memmy_Status MemmyCli_Value_Format(Arena *arena, MemmyCli_ValueFormat *format, String8 bytes, String8 *out,
                                   Memmy_Error *error)
{
    String8List parts = {0};
    Memmy_Status status = MemmyCli_PeekValue_Format(arena, format, bytes, &parts, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = String8List_Join(arena, &parts, (String8){0});
    return Memmy_Status_Ok;
}
