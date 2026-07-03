#include "memmy_value.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static B32 Memmy_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void Memmy_Error_SetInput(Memmy_Error *error, Memmy_Status status, String8 context, String8 message,
                                 String8 input, U64 byte_offset, U64 byte_count)
{
    if (error != 0)
    {
        *error = (Memmy_Error){
            .status = status,
            .message = message,
            .input = input,
            .byte_offset = byte_offset,
            .byte_count = byte_count,
            .context = context,
        };
    }
}

static U32 Memmy_HexDigitValue(U8 c)
{
    U32 result = U32_MAX;
    if (c >= '0' && c <= '9')
    {
        result = (U32)(c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
        result = 10u + (U32)(c - 'a');
    }
    else if (c >= 'A' && c <= 'F')
    {
        result = 10u + (U32)(c - 'A');
    }
    return result;
}

static void Memmy_WriteU64LE(U8 *dst, U64 value, U64 size)
{
    for (U64 i = 0; i < size; i++)
    {
        dst[i] = (U8)(value >> (i * 8));
    }
}

static Memmy_Status Memmy_ParseUnsigned(String8 text, U64 max_value, U64 *out, U64 *out_error_offset)
{
    if (text.len == 0 || text.data[0] == '-' || text.data[0] == '+')
    {
        *out_error_offset = 0;
        return Memmy_Status_ParseError;
    }

    U32 base = 10;
    U64 first_digit = 0;
    if (text.len >= 2 && text.data[0] == '0' && (text.data[1] == 'x' || text.data[1] == 'X'))
    {
        base = 16;
        first_digit = 2;
        if (text.len == 2)
        {
            *out_error_offset = 2;
            return Memmy_Status_ParseError;
        }
    }

    U64 value = 0;
    for (U64 i = first_digit; i < text.len; i++)
    {
        U32 digit = (base == 16) ? Memmy_HexDigitValue(text.data[i])
                                 : (Char8_IsDigit(text.data[i]) ? (U32)(text.data[i] - '0') : U32_MAX);
        if (digit >= base)
        {
            *out_error_offset = i;
            return Memmy_Status_ParseError;
        }
        if (value > (max_value - digit) / base)
        {
            *out_error_offset = i;
            return Memmy_Status_Overflow;
        }
        value = value * base + digit;
    }

    *out = value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_ParseSigned(String8 text, I64 min_value, I64 max_value, I64 *out, U64 *out_error_offset)
{
    if (text.len == 0)
    {
        *out_error_offset = 0;
        return Memmy_Status_ParseError;
    }

    B32 negative = 0;
    U64 first_digit = 0;
    if (text.data[0] == '-')
    {
        negative = 1;
        first_digit = 1;
    }
    else if (text.data[0] == '+')
    {
        first_digit = 1;
    }
    if (first_digit == text.len)
    {
        *out_error_offset = first_digit;
        return Memmy_Status_ParseError;
    }

    U64 limit = negative ? (U64)(-(min_value + 1)) + 1 : (U64)max_value;
    U64 magnitude = 0;
    String8 digits = String8_Substr(text, first_digit, text.len - first_digit);
    U64 local_offset = 0;
    Memmy_Status status = Memmy_ParseUnsigned(digits, limit, &magnitude, &local_offset);
    if (status != Memmy_Status_Ok)
    {
        *out_error_offset = first_digit + local_offset;
        return status;
    }

    if (negative)
    {
        *out = (magnitude == limit && min_value == I64_MIN) ? I64_MIN : -(I64)magnitude;
    }
    else
    {
        *out = (I64)magnitude;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EncodeUnsigned(Arena *arena, Memmy_Type type, String8 text, U64 max_value, U64 size,
                                         Memmy_Value *out, Memmy_Error *error)
{
    U64 value = 0;
    U64 error_offset = 0;
    Memmy_Status status = Memmy_ParseUnsigned(text, max_value, &value, &error_offset);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Error_SetInput(error, status, String8_Lit("value"),
                             status == Memmy_Status_Overflow ? String8_Lit("unsigned integer overflow")
                                                             : String8_Lit("invalid unsigned integer"),
                             text, error_offset, 1);
        return status;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    Memmy_WriteU64LE(bytes, value, size);
    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, size)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EncodeSigned(Arena *arena, Memmy_Type type, String8 text, I64 min_value, I64 max_value,
                                       U64 size, Memmy_Value *out, Memmy_Error *error)
{
    I64 value = 0;
    U64 error_offset = 0;
    Memmy_Status status = Memmy_ParseSigned(text, min_value, max_value, &value, &error_offset);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Error_SetInput(error, status, String8_Lit("value"),
                             status == Memmy_Status_Overflow ? String8_Lit("signed integer overflow")
                                                             : String8_Lit("invalid signed integer"),
                             text, error_offset, 1);
        return status;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    Memmy_WriteU64LE(bytes, (U64)value, size);
    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, size)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EncodeFloat(Arena *arena, Memmy_Type type, String8 text, U64 size, Memmy_Value *out,
                                      Memmy_Error *error)
{
    Scratch scratch = Scratch_Begin(&arena, 1);
    char *cstr = String8_ToCStr(scratch.arena, text);
    char *end = 0;
    errno = 0;
    double parsed = strtod(cstr, &end);
    if (end == cstr || *end != 0 || errno == ERANGE)
    {
        U64 offset = (end > cstr) ? (U64)(end - cstr) : 0;
        Scratch_End(scratch);
        Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("value"), String8_Lit("invalid float"), text,
                             offset, 1);
        return Memmy_Status_ParseError;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    if (size == 4)
    {
        F32 value = (F32)parsed;
        memcpy(bytes, &value, sizeof(value));
    }
    else
    {
        F64 value = parsed;
        memcpy(bytes, &value, sizeof(value));
    }
    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, size)};
    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Utf8Next(String8 text, U64 *cursor, U32 *out_codepoint, Memmy_Error *error)
{
    U64 i = *cursor;
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
        Memmy_Error_SetInput(error, Memmy_Status_InvalidEncoding, String8_Lit("value"),
                             String8_Lit("invalid utf-8 leading byte"), text, i, 1);
        return Memmy_Status_InvalidEncoding;
    }

    if (i + need > text.len)
    {
        Memmy_Error_SetInput(error, Memmy_Status_InvalidEncoding, String8_Lit("value"),
                             String8_Lit("truncated utf-8 sequence"), text, i, text.len - i);
        return Memmy_Status_InvalidEncoding;
    }
    for (U64 j = 1; j < need; j++)
    {
        U8 c = text.data[i + j];
        if ((c & 0xc0) != 0x80)
        {
            Memmy_Error_SetInput(error, Memmy_Status_InvalidEncoding, String8_Lit("value"),
                                 String8_Lit("invalid utf-8 continuation byte"), text, i + j, 1);
            return Memmy_Status_InvalidEncoding;
        }
        codepoint = (codepoint << 6) | (c & 0x3f);
    }

    if ((need == 3 && codepoint < 0x800) || (need == 4 && codepoint < 0x10000) ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff)
    {
        Memmy_Error_SetInput(error, Memmy_Status_InvalidEncoding, String8_Lit("value"),
                             String8_Lit("invalid utf-8 codepoint"), text, i, need);
        return Memmy_Status_InvalidEncoding;
    }

    *cursor += need;
    *out_codepoint = codepoint;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EncodeWStr(Arena *arena, Memmy_Type type, String8 text, Memmy_Value *out, Memmy_Error *error)
{
    U64 unit_count = 0;
    U64 cursor = 0;
    while (cursor < text.len)
    {
        U32 cp = 0;
        Memmy_Status status = Memmy_Utf8Next(text, &cursor, &cp, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        unit_count += (cp > 0xffff) ? 2 : 1;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, unit_count * 2);
    U64 at = 0;
    cursor = 0;
    while (cursor < text.len)
    {
        U32 cp = 0;
        Memmy_Utf8Next(text, &cursor, &cp, 0);
        if (cp <= 0xffff)
        {
            bytes[at++] = (U8)cp;
            bytes[at++] = (U8)(cp >> 8);
        }
        else
        {
            cp -= 0x10000;
            U32 high = 0xd800 + (cp >> 10);
            U32 low = 0xdc00 + (cp & 0x3ff);
            bytes[at++] = (U8)high;
            bytes[at++] = (U8)(high >> 8);
            bytes[at++] = (U8)low;
            bytes[at++] = (U8)(low >> 8);
        }
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, unit_count * 2)};
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Type_Parse(String8 text, Memmy_Type *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("type"), String8_Lit("missing output type"));
        return Memmy_Status_InvalidArgument;
    }

    struct
    {
        String8 name;
        Memmy_TypeKind kind;
        U64 fixed_size;
    } table[] = {
        {String8_Lit("u8"), Memmy_TypeKind_U8, 1},   {String8_Lit("i8"), Memmy_TypeKind_I8, 1},
        {String8_Lit("u16"), Memmy_TypeKind_U16, 2}, {String8_Lit("i16"), Memmy_TypeKind_I16, 2},
        {String8_Lit("u32"), Memmy_TypeKind_U32, 4}, {String8_Lit("i32"), Memmy_TypeKind_I32, 4},
        {String8_Lit("u64"), Memmy_TypeKind_U64, 8}, {String8_Lit("i64"), Memmy_TypeKind_I64, 8},
        {String8_Lit("f32"), Memmy_TypeKind_F32, 4}, {String8_Lit("f64"), Memmy_TypeKind_F64, 8},
        {String8_Lit("ptr"), Memmy_TypeKind_Ptr, 0}, {String8_Lit("bytes"), Memmy_TypeKind_Bytes, 0},
        {String8_Lit("str"), Memmy_TypeKind_Str, 0}, {String8_Lit("wstr"), Memmy_TypeKind_WStr, 0},
    };

    for (U64 i = 0; i < ArrayCount(table); i++)
    {
        if (String8_Eq(text, table[i].name))
        {
            *out = (Memmy_Type){.kind = table[i].kind, .fixed_size = table[i].fixed_size};
            if (error != 0)
            {
                *error = (Memmy_Error){0};
            }
            return Memmy_Status_Ok;
        }
    }

    Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("type"), String8_Lit("unknown type"), text, 0,
                         text.len);
    return Memmy_Status_ParseError;
}

Memmy_Status Memmy_Pattern_Parse(Arena *arena, String8 text, Memmy_PatternParseFlags flags, Memmy_Pattern *out,
                                 Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("pattern"),
                        String8_Lit("missing arena or output pattern"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (Memmy_Pattern){0};
    U64 count = 0;
    U64 cursor = 0;
    while (cursor < text.len)
    {
        while (cursor < text.len && Memmy_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        if (cursor == text.len)
        {
            break;
        }
        count++;
        while (cursor < text.len && !Memmy_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
    }

    if (count == 0)
    {
        Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("pattern"), String8_Lit("expected pattern"),
                             text, 0, 0);
        return Memmy_Status_ParseError;
    }

    Memmy_PatternByte *bytes = Arena_PushArray(arena, Memmy_PatternByte, count);
    cursor = 0;
    U64 index = 0;
    while (cursor < text.len)
    {
        while (cursor < text.len && Memmy_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        if (cursor == text.len)
        {
            break;
        }
        U64 start = cursor;
        while (cursor < text.len && !Memmy_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        String8 token = String8_Substr(text, start, cursor - start);

        if (String8_Eq(token, String8_Lit("?")) || String8_Eq(token, String8_Lit("??")))
        {
            if ((flags & Memmy_PatternParseFlag_AllowWildcards) == 0)
            {
                Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("pattern"),
                                     String8_Lit("wildcards are not allowed"), text, start, token.len);
                return Memmy_Status_ParseError;
            }
            bytes[index++] = (Memmy_PatternByte){.wildcard = 1};
            continue;
        }
        if (token.len != 2)
        {
            Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("pattern"),
                                 String8_Lit("expected two-digit byte token"), text, start, token.len);
            return Memmy_Status_ParseError;
        }

        U32 hi = Memmy_HexDigitValue(token.data[0]);
        U32 lo = Memmy_HexDigitValue(token.data[1]);
        if (hi >= 16 || lo >= 16)
        {
            Memmy_Error_SetInput(error, Memmy_Status_ParseError, String8_Lit("pattern"),
                                 String8_Lit("invalid hexadecimal byte"), text, start, token.len);
            return Memmy_Status_ParseError;
        }
        bytes[index++] = (Memmy_PatternByte){.value = (U8)((hi << 4) | lo)};
    }

    *out = (Memmy_Pattern){.bytes = bytes, .count = count};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Value_Parse(Arena *arena, Memmy_Type type, Memmy_PointerWidth pointer_width, String8 text,
                               Memmy_Value *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("value"),
                        String8_Lit("missing arena or output value"));
        return Memmy_Status_InvalidArgument;
    }

    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
        return Memmy_EncodeUnsigned(arena, type, text, U8_MAX, 1, out, error);
    case Memmy_TypeKind_I8:
        return Memmy_EncodeSigned(arena, type, text, I8_MIN, I8_MAX, 1, out, error);
    case Memmy_TypeKind_U16:
        return Memmy_EncodeUnsigned(arena, type, text, U16_MAX, 2, out, error);
    case Memmy_TypeKind_I16:
        return Memmy_EncodeSigned(arena, type, text, I16_MIN, I16_MAX, 2, out, error);
    case Memmy_TypeKind_U32:
        return Memmy_EncodeUnsigned(arena, type, text, U32_MAX, 4, out, error);
    case Memmy_TypeKind_I32:
        return Memmy_EncodeSigned(arena, type, text, I32_MIN, I32_MAX, 4, out, error);
    case Memmy_TypeKind_U64:
        return Memmy_EncodeUnsigned(arena, type, text, U64_MAX, 8, out, error);
    case Memmy_TypeKind_I64:
        return Memmy_EncodeSigned(arena, type, text, I64_MIN, I64_MAX, 8, out, error);
    case Memmy_TypeKind_F32:
        return Memmy_EncodeFloat(arena, type, text, 4, out, error);
    case Memmy_TypeKind_F64:
        return Memmy_EncodeFloat(arena, type, text, 8, out, error);
    case Memmy_TypeKind_Ptr: {
        U64 size = pointer_width == Memmy_PointerWidth_32 ? 4 : pointer_width == Memmy_PointerWidth_64 ? 8 : 0;
        if (size == 0)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("value"),
                            String8_Lit("unknown pointer width"));
            return Memmy_Status_InvalidArgument;
        }
        U64 max_value = size == 4 ? U32_MAX : U64_MAX;
        return Memmy_EncodeUnsigned(arena, type, text, max_value, size, out, error);
    }
    case Memmy_TypeKind_Bytes: {
        Memmy_Pattern pattern = {0};
        Memmy_Status status = Memmy_Pattern_Parse(arena, text, 0, &pattern, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        U8 *bytes = Arena_PushArrayNoZero(arena, U8, pattern.count);
        for (U64 i = 0; i < pattern.count; i++)
        {
            bytes[i] = pattern.bytes[i].value;
        }
        *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, pattern.count)};
        return Memmy_Status_Ok;
    }
    case Memmy_TypeKind_Str:
        *out = (Memmy_Value){.type = type, .bytes = String8_Copy(arena, text)};
        return Memmy_Status_Ok;
    case Memmy_TypeKind_WStr:
        return Memmy_EncodeWStr(arena, type, text, out, error);
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("value"), String8_Lit("unknown value type"));
    return Memmy_Status_InvalidArgument;
}
