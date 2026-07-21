#include "memmy_value.h"

#include "base.h"

#include <float.h>
#include <math.h>

#define MEMMY_INTEGER_TYPE(bits, sign)                                                                                 \
    {                                                                                                                  \
        .kind = Memmy_TypeKind_Integer, .integer = {.bit_count = (bits), .is_signed = (sign) }                         \
    }
#define MEMMY_FLOAT_TYPE(bits)                                                                                         \
    {                                                                                                                  \
        .kind = Memmy_TypeKind_Float, .floating = {.bit_count = (bits) }                                               \
    }
#define MEMMY_STRING_TYPE(enc) {.kind = Memmy_TypeKind_String, .string = {.encoding = (enc), .zero_terminated = 1}}

Memmy_Type const Memmy_Type_Null = {.kind = Memmy_TypeKind_Null};
Memmy_Type const Memmy_Type_U8 = MEMMY_INTEGER_TYPE(8, 0);
Memmy_Type const Memmy_Type_I8 = MEMMY_INTEGER_TYPE(8, 1);
Memmy_Type const Memmy_Type_U16 = MEMMY_INTEGER_TYPE(16, 0);
Memmy_Type const Memmy_Type_I16 = MEMMY_INTEGER_TYPE(16, 1);
Memmy_Type const Memmy_Type_U32 = MEMMY_INTEGER_TYPE(32, 0);
Memmy_Type const Memmy_Type_I32 = MEMMY_INTEGER_TYPE(32, 1);
Memmy_Type const Memmy_Type_U64 = MEMMY_INTEGER_TYPE(64, 0);
Memmy_Type const Memmy_Type_I64 = MEMMY_INTEGER_TYPE(64, 1);
Memmy_Type const Memmy_Type_F32 = MEMMY_FLOAT_TYPE(32);
Memmy_Type const Memmy_Type_F64 = MEMMY_FLOAT_TYPE(64);
Memmy_Type const Memmy_Type_Address = {.kind = Memmy_TypeKind_Address};
Memmy_Type const Memmy_Type_Str = MEMMY_STRING_TYPE(Memmy_StringEncoding_Utf8);
Memmy_Type const Memmy_Type_WStr = MEMMY_STRING_TYPE(Memmy_StringEncoding_Utf16Le);
Memmy_Type const Memmy_Type_Range = {.kind = Memmy_TypeKind_Range};

static void Memmy_Value_Error(Memmy_Error *error, Memmy_Status status, String8 message)
{
    Memmy_Error_Set(error, status, String8_Lit("value"), message);
}

static void Memmy_Value_ErrorInput(Memmy_Error *error, Memmy_Status status, String8 message, String8 input, U64 offset,
                                   U64 count)
{
    if (error != 0)
    {
        *error = (Memmy_Error){
            .status = status,
            .message = message,
            .input = input,
            .byte_offset = offset,
            .byte_count = count,
            .context = String8_Lit("value"),
        };
    }
}

static B32 Memmy_Type_IntegerWidthIsValid(U32 bit_count)
{
    return bit_count == 8 || bit_count == 16 || bit_count == 32 || bit_count == 64;
}

B32 Memmy_Type_IsValid(Memmy_Type type)
{
    switch (type.kind)
    {
    case Memmy_TypeKind_Null:
    case Memmy_TypeKind_Address:
    case Memmy_TypeKind_Range:
        return 1;
    case Memmy_TypeKind_Integer:
        return Memmy_Type_IntegerWidthIsValid(type.integer.bit_count) &&
               (type.integer.is_signed == 0 || type.integer.is_signed == 1);
    case Memmy_TypeKind_Float:
        return type.floating.bit_count == 32 || type.floating.bit_count == 64;
    case Memmy_TypeKind_String:
        return (type.string.encoding == Memmy_StringEncoding_Utf8 ||
                type.string.encoding == Memmy_StringEncoding_Utf16Le) &&
               (type.string.zero_terminated == 0 || type.string.zero_terminated == 1);
    case Memmy_TypeKind_List:
        return type.list.element_type != 0 && Memmy_Type_IsValid(*type.list.element_type) &&
               type.list.element_type->kind != Memmy_TypeKind_Null &&
               type.list.element_type->kind != Memmy_TypeKind_List;
    }
    return 0;
}

B32 Memmy_Type_IsNull(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_Null;
}

B32 Memmy_Type_IsInteger(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_Integer && Memmy_Type_IsValid(type);
}

B32 Memmy_Type_IsFloat(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_Float && Memmy_Type_IsValid(type);
}

B32 Memmy_Type_IsAddress(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_Address;
}

B32 Memmy_Type_IsString(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_String && Memmy_Type_IsValid(type);
}

B32 Memmy_Type_IsRange(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_Range;
}

B32 Memmy_Type_IsList(Memmy_Type type)
{
    return type.kind == Memmy_TypeKind_List && Memmy_Type_IsValid(type);
}

B32 Memmy_Type_Eq(Memmy_Type a, Memmy_Type b)
{
    if (!Memmy_Type_IsValid(a) || !Memmy_Type_IsValid(b) || a.kind != b.kind)
    {
        return 0;
    }
    switch (a.kind)
    {
    case Memmy_TypeKind_Integer:
        return a.integer.bit_count == b.integer.bit_count && a.integer.is_signed == b.integer.is_signed;
    case Memmy_TypeKind_Float:
        return a.floating.bit_count == b.floating.bit_count;
    case Memmy_TypeKind_String:
        return a.string.encoding == b.string.encoding && a.string.zero_terminated == b.string.zero_terminated;
    case Memmy_TypeKind_List:
        return Memmy_Type_Eq(*a.list.element_type, *b.list.element_type);
    default:
        return 1;
    }
}

U64 Memmy_Type_EncodedSize(Memmy_Type type)
{
    if (Memmy_Type_IsInteger(type))
    {
        return type.integer.bit_count / 8;
    }
    if (Memmy_Type_IsFloat(type))
    {
        return type.floating.bit_count / 8;
    }
    return 0;
}

Memmy_Status Memmy_Type_ListCreate(Arena *arena, Memmy_Type element_type, Memmy_Type *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Type){0};
    }
    if (arena == 0 || out == 0 || !Memmy_Type_IsValid(element_type) || Memmy_Type_IsNull(element_type) ||
        Memmy_Type_IsList(element_type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid list element type"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Type *element_copy = (Memmy_Type *)Arena_Push(arena, sizeof(Memmy_Type), _Alignof(Memmy_Type));
    *element_copy = element_type;
    *out = (Memmy_Type){.kind = Memmy_TypeKind_List, .list = {.element_type = element_copy}};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Type_Parse(String8 text, Memmy_Type *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("missing output type"));
        return Memmy_Status_InvalidArgument;
    }
    *out = (Memmy_Type){0};
    struct
    {
        String8 name;
        Memmy_Type const *type;
    } table[] = {
        {String8_Lit("u8"), &Memmy_Type_U8},   {String8_Lit("i8"), &Memmy_Type_I8},
        {String8_Lit("u16"), &Memmy_Type_U16}, {String8_Lit("i16"), &Memmy_Type_I16},
        {String8_Lit("u32"), &Memmy_Type_U32}, {String8_Lit("i32"), &Memmy_Type_I32},
        {String8_Lit("u64"), &Memmy_Type_U64}, {String8_Lit("i64"), &Memmy_Type_I64},
        {String8_Lit("f32"), &Memmy_Type_F32}, {String8_Lit("f64"), &Memmy_Type_F64},
        {String8_Lit("str"), &Memmy_Type_Str}, {String8_Lit("wstr"), &Memmy_Type_WStr},
    };
    for (U64 i = 0; i < ArrayCount(table); i++)
    {
        if (String8_Eq(text, table[i].name))
        {
            *out = *table[i].type;
            if (error != 0)
            {
                *error = (Memmy_Error){0};
            }
            return Memmy_Status_Ok;
        }
    }
    if (error != 0)
    {
        *error = (Memmy_Error){.status = Memmy_Status_ParseError,
                               .message = String8_Lit("unknown type"),
                               .input = text,
                               .byte_count = text.len,
                               .context = String8_Lit("type")};
    }
    return Memmy_Status_ParseError;
}

static Memmy_Status Memmy_Type_Copy(Arena *arena, Memmy_Type type, Memmy_Type *out)
{
    if (!Memmy_Type_IsValid(type))
    {
        return Memmy_Status_InvalidArgument;
    }
    *out = type;
    if (type.kind == Memmy_TypeKind_List)
    {
        Memmy_Type *element = (Memmy_Type *)Arena_Push(arena, sizeof(Memmy_Type), _Alignof(Memmy_Type));
        *element = *type.list.element_type;
        out->list.element_type = element;
    }
    return Memmy_Status_Ok;
}

static U64 Memmy_Value_ListElementSize(Memmy_Type type)
{
    if (type.kind == Memmy_TypeKind_Integer)
    {
        return sizeof(U64);
    }
    if (type.kind == Memmy_TypeKind_Float)
    {
        return type.floating.bit_count == 32 ? sizeof(U32) : sizeof(U64);
    }
    if (type.kind == Memmy_TypeKind_Address)
    {
        return sizeof(Memmy_Addr);
    }
    if (type.kind == Memmy_TypeKind_Range)
    {
        return sizeof(Memmy_Range);
    }
    if (type.kind == Memmy_TypeKind_String)
    {
        return sizeof(String8);
    }
    return 0;
}

Memmy_Status Memmy_Value_Copy(Arena *arena, Memmy_Value const *value, Memmy_Value *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Value){0};
    }
    if (arena == 0 || value == 0 || out == 0 || !Memmy_Type_IsValid(value->type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid value copy arguments"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Value copy = *value;
    Memmy_Status status = Memmy_Type_Copy(arena, value->type, &copy.type);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (value->type.kind == Memmy_TypeKind_String)
    {
        copy.string = String8_Copy(arena, value->string);
    }
    else if (value->type.kind == Memmy_TypeKind_List)
    {
        Memmy_Type element = *value->type.list.element_type;
        U64 element_size = Memmy_Value_ListElementSize(element);
        if ((value->list.count != 0 && value->list.unsigned_integers == 0) || element_size == 0)
        {
            Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid list payload"));
            return Memmy_Status_InvalidArgument;
        }
        void *items = Arena_Push(arena, element_size * value->list.count, 8);
        if (value->list.count != 0)
        {
            Memory_Copy(items, value->list.unsigned_integers, element_size * value->list.count);
        }
        copy.list.unsigned_integers = (U64 *)items;
        if (element.kind == Memmy_TypeKind_String)
        {
            for (U64 i = 0; i < value->list.count; i++)
            {
                copy.list.strings[i] = String8_Copy(arena, value->list.strings[i]);
            }
        }
    }
    *out = copy;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

static U64 Memmy_Integer_ReadLe(String8 bytes)
{
    U64 result = 0;
    for (U64 i = 0; i < bytes.len; i++)
    {
        result |= (U64)bytes.data[i] << (i * 8);
    }
    return result;
}

static void Memmy_Integer_WriteLe(U8 *bytes, U64 size, U64 value)
{
    for (U64 i = 0; i < size; i++)
    {
        bytes[i] = (U8)(value >> (i * 8));
    }
}

static I64 Memmy_Integer_SignExtend(U64 value, U32 bit_count)
{
    if (bit_count == 64)
    {
        return (I64)value;
    }
    U64 sign = 1ull << (bit_count - 1);
    return (I64)((value ^ sign) - sign);
}

static Memmy_Status Memmy_Utf8_Next(String8 text, U64 *cursor, U32 *out_codepoint, Memmy_Error *error)
{
    U64 start = *cursor;
    U8 first = text.data[start];
    U64 need = 0;
    U32 cp = 0;
    if (first < 0x80)
    {
        need = 1;
        cp = first;
    }
    else if (first >= 0xc2 && first <= 0xdf)
    {
        need = 2;
        cp = first & 0x1f;
    }
    else if (first >= 0xe0 && first <= 0xef)
    {
        need = 3;
        cp = first & 0x0f;
    }
    else if (first >= 0xf0 && first <= 0xf4)
    {
        need = 4;
        cp = first & 7;
    }
    else
    {
        Memmy_Value_ErrorInput(error, Memmy_Status_InvalidEncoding, String8_Lit("invalid utf-8 leading byte"), text,
                               start, 1);
        return Memmy_Status_InvalidEncoding;
    }
    if (start + need > text.len)
    {
        Memmy_Value_ErrorInput(error, Memmy_Status_InvalidEncoding, String8_Lit("truncated utf-8 sequence"), text,
                               start, text.len - start);
        return Memmy_Status_InvalidEncoding;
    }
    for (U64 i = 1; i < need; i++)
    {
        U8 byte = text.data[start + i];
        if ((byte & 0xc0) != 0x80)
        {
            Memmy_Value_ErrorInput(error, Memmy_Status_InvalidEncoding, String8_Lit("invalid utf-8 continuation"), text,
                                   start + i, 1);
            return Memmy_Status_InvalidEncoding;
        }
        cp = (cp << 6) | (byte & 0x3f);
    }
    if ((need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff)
    {
        Memmy_Value_ErrorInput(error, Memmy_Status_InvalidEncoding, String8_Lit("invalid utf-8 codepoint"), text, start,
                               need);
        return Memmy_Status_InvalidEncoding;
    }
    *cursor += need;
    *out_codepoint = cp;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Utf8_Validate(String8 text, Memmy_Error *error)
{
    U64 cursor = 0;
    while (cursor < text.len)
    {
        U32 cp = 0;
        Memmy_Status status = Memmy_Utf8_Next(text, &cursor, &cp, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    return Memmy_Status_Ok;
}

static U64 Memmy_Utf8_EncodedSize(U32 cp)
{
    return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
}

static void Memmy_Utf8_Write(U8 *bytes, U64 *at, U32 cp)
{
    if (cp < 0x80)
    {
        bytes[(*at)++] = (U8)cp;
    }
    else if (cp < 0x800)
    {
        bytes[(*at)++] = (U8)(0xc0 | (cp >> 6));
        bytes[(*at)++] = (U8)(0x80 | (cp & 0x3f));
    }
    else if (cp < 0x10000)
    {
        bytes[(*at)++] = (U8)(0xe0 | (cp >> 12));
        bytes[(*at)++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
        bytes[(*at)++] = (U8)(0x80 | (cp & 0x3f));
    }
    else
    {
        bytes[(*at)++] = (U8)(0xf0 | (cp >> 18));
        bytes[(*at)++] = (U8)(0x80 | ((cp >> 12) & 0x3f));
        bytes[(*at)++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
        bytes[(*at)++] = (U8)(0x80 | (cp & 0x3f));
    }
}

static Memmy_Status Memmy_Utf16Le_Decode(Arena *arena, String8 encoded, String8 *out, Memmy_Error *error)
{
    if ((encoded.len & 1) != 0)
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("truncated utf-16 code unit"));
        return Memmy_Status_InvalidEncoding;
    }
    U64 utf8_size = 0;
    for (U64 i = 0; i < encoded.len; i += 2)
    {
        U32 cp = (U32)(encoded.data[i] | (encoded.data[i + 1] << 8));
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            if (i + 3 >= encoded.len)
            {
                Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("truncated utf-16 surrogate"));
                return Memmy_Status_InvalidEncoding;
            }
            U32 low = (U32)(encoded.data[i + 2] | (encoded.data[i + 3] << 8));
            if (low < 0xdc00 || low > 0xdfff)
            {
                Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("invalid utf-16 surrogate"));
                return Memmy_Status_InvalidEncoding;
            }
            cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
            i += 2;
        }
        else if (cp >= 0xdc00 && cp <= 0xdfff)
        {
            Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("unpaired utf-16 surrogate"));
            return Memmy_Status_InvalidEncoding;
        }
        utf8_size += Memmy_Utf8_EncodedSize(cp);
    }
    U8 *bytes = Arena_PushArrayNoZero(arena, U8, utf8_size);
    U64 at = 0;
    for (U64 i = 0; i < encoded.len; i += 2)
    {
        U32 cp = (U32)(encoded.data[i] | (encoded.data[i + 1] << 8));
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            U32 low = (U32)(encoded.data[i + 2] | (encoded.data[i + 3] << 8));
            cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
            i += 2;
        }
        Memmy_Utf8_Write(bytes, &at, cp);
    }
    *out = String8_Make(bytes, at);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Value_Decode(Arena *arena, Memmy_Type type, String8 encoded, Memmy_Value *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Value){0};
    }
    if (arena == 0 || out == 0 || !Memmy_Type_IsValid(type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid decode arguments"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Value result = {.type = type};
    if (Memmy_Type_IsInteger(type))
    {
        if (encoded.len != type.integer.bit_count / 8)
        {
            Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("incorrect integer byte count"));
            return Memmy_Status_InvalidArgument;
        }
        U64 raw = Memmy_Integer_ReadLe(encoded);
        if (type.integer.is_signed)
        {
            result.signed_integer = Memmy_Integer_SignExtend(raw, type.integer.bit_count);
        }
        else
        {
            result.unsigned_integer = raw;
        }
    }
    else if (Memmy_Type_IsFloat(type))
    {
        if (encoded.len != type.floating.bit_count / 8)
        {
            Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("incorrect float byte count"));
            return Memmy_Status_InvalidArgument;
        }
        result.floating_bits = Memmy_Integer_ReadLe(encoded);
    }
    else if (Memmy_Type_IsString(type))
    {
        U64 terminator_size = type.string.encoding == Memmy_StringEncoding_Utf8 ? 1 : 2;
        if (type.string.zero_terminated)
        {
            if (encoded.len < terminator_size)
            {
                Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("missing string terminator"));
                return Memmy_Status_InvalidEncoding;
            }
            for (U64 i = 0; i < terminator_size; i++)
            {
                if (encoded.data[encoded.len - terminator_size + i] != 0)
                {
                    Memmy_Value_Error(error, Memmy_Status_InvalidEncoding, String8_Lit("missing string terminator"));
                    return Memmy_Status_InvalidEncoding;
                }
            }
            encoded.len -= terminator_size;
        }
        if (type.string.encoding == Memmy_StringEncoding_Utf8)
        {
            Memmy_Status status = Memmy_Utf8_Validate(encoded, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            result.string = String8_Copy(arena, encoded);
        }
        else
        {
            Memmy_Status status = Memmy_Utf16Le_Decode(arena, encoded, &result.string, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
    }
    else
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("type is not decodable"));
        return Memmy_Status_InvalidArgument;
    }
    *out = result;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

static U64 Memmy_Integer_Mask(U32 bit_count)
{
    return bit_count == 64 ? U64_MAX : (1ull << bit_count) - 1;
}

static I64 Memmy_Integer_SignedMin(U32 bit_count)
{
    return bit_count == 64 ? I64_MIN : -(I64)(1ull << (bit_count - 1));
}

static I64 Memmy_Integer_SignedMax(U32 bit_count)
{
    return bit_count == 64 ? I64_MAX : (I64)((1ull << (bit_count - 1)) - 1);
}

static F64 Memmy_Value_FloatAsF64(Memmy_Value const *value)
{
    if (value->type.floating.bit_count == 32)
    {
        U32 bits = (U32)value->floating_bits;
        F32 number = 0;
        Memory_Copy(&number, &bits, sizeof(number));
        return number;
    }
    F64 number = 0;
    Memory_Copy(&number, &value->floating_bits, sizeof(number));
    return number;
}

static U64 Memmy_Value_FloatBitsFromF64(F64 number, U32 bit_count)
{
    U64 bits = 0;
    if (bit_count == 32)
    {
        F32 narrowed = (F32)number;
        U32 bits32 = 0;
        Memory_Copy(&bits32, &narrowed, sizeof(bits32));
        bits = bits32;
    }
    else
    {
        Memory_Copy(&bits, &number, sizeof(bits));
    }
    return bits;
}

Memmy_Status Memmy_Value_Convert(Arena *arena, Memmy_Value const *value, Memmy_Type destination_type, Memmy_Value *out,
                                 Memmy_Error *error)
{
    Memmy_Value input = {0};
    if (value != 0)
    {
        input = *value;
        value = &input;
    }
    if (out != 0)
    {
        *out = (Memmy_Value){0};
    }
    if (arena == 0 || value == 0 || out == 0 || !Memmy_Type_IsValid(value->type) ||
        !Memmy_Type_IsValid(destination_type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid conversion arguments"));
        return Memmy_Status_InvalidArgument;
    }
    if (Memmy_Type_Eq(value->type, destination_type))
    {
        return Memmy_Value_Copy(arena, value, out, error);
    }
    Memmy_Value result = {.type = destination_type};
    if (Memmy_Type_IsInteger(value->type) && Memmy_Type_IsInteger(destination_type))
    {
        U64 raw = value->type.integer.is_signed ? (U64)value->signed_integer : value->unsigned_integer;
        if (destination_type.integer.is_signed)
        {
            I64 max = Memmy_Integer_SignedMax(destination_type.integer.bit_count);
            I64 min = Memmy_Integer_SignedMin(destination_type.integer.bit_count);
            if ((value->type.integer.is_signed && (value->signed_integer < min || value->signed_integer > max)) ||
                (!value->type.integer.is_signed && value->unsigned_integer > (U64)max))
            {
                Memmy_Value_Error(error, Memmy_Status_Overflow, String8_Lit("integer conversion overflow"));
                return Memmy_Status_Overflow;
            }
            result.signed_integer = value->type.integer.is_signed ? value->signed_integer : (I64)raw;
        }
        else
        {
            result.unsigned_integer = raw & Memmy_Integer_Mask(destination_type.integer.bit_count);
        }
    }
    else if ((Memmy_Type_IsInteger(value->type) || Memmy_Type_IsFloat(value->type)) &&
             (Memmy_Type_IsInteger(destination_type) || Memmy_Type_IsFloat(destination_type)))
    {
        F64 number = 0;
        if (Memmy_Type_IsFloat(value->type))
        {
            number = Memmy_Value_FloatAsF64(value);
        }
        else
        {
            number = value->type.integer.is_signed ? (F64)value->signed_integer : (F64)value->unsigned_integer;
        }
        if (Memmy_Type_IsFloat(destination_type))
        {
            if (destination_type.floating.bit_count == 32 && isfinite(number) &&
                (number > FLT_MAX || number < -FLT_MAX))
            {
                Memmy_Value_Error(error, Memmy_Status_Overflow, String8_Lit("float conversion overflow"));
                return Memmy_Status_Overflow;
            }
            result.floating_bits = Memmy_Value_FloatBitsFromF64(number, destination_type.floating.bit_count);
        }
        else
        {
            if (!isfinite(number))
            {
                Memmy_Value_Error(error, Memmy_Status_Overflow, String8_Lit("float-to-integer conversion overflow"));
                return Memmy_Status_Overflow;
            }
            if (destination_type.integer.is_signed)
            {
                I64 min = Memmy_Integer_SignedMin(destination_type.integer.bit_count);
                I64 max = Memmy_Integer_SignedMax(destination_type.integer.bit_count);
                if (number < (F64)min || number >= (destination_type.integer.bit_count == 64 ? 0x1p63 : (F64)max + 1.0))
                {
                    Memmy_Value_Error(error, Memmy_Status_Overflow,
                                      String8_Lit("float-to-integer conversion overflow"));
                    return Memmy_Status_Overflow;
                }
                result.signed_integer = (I64)number;
            }
            else
            {
                F64 upper = destination_type.integer.bit_count == 64
                                ? 0x1p64
                                : (F64)(1ull << destination_type.integer.bit_count);
                if (number <= -1.0 || number >= upper)
                {
                    Memmy_Value_Error(error, Memmy_Status_Overflow,
                                      String8_Lit("float-to-integer conversion overflow"));
                    return Memmy_Status_Overflow;
                }
                result.unsigned_integer = (U64)number;
            }
        }
    }
    else if (Memmy_Type_IsString(value->type) && Memmy_Type_IsString(destination_type))
    {
        Memmy_Status status = Memmy_Utf8_Validate(value->string, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result.string = String8_Copy(arena, value->string);
    }
    else
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("unsupported semantic conversion"));
        return Memmy_Status_InvalidArgument;
    }
    *out = result;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Utf16Le_Encode(Arena *arena, String8 text, B32 terminated, String8 *out, Memmy_Error *error)
{
    U64 units = terminated ? 1 : 0;
    U64 cursor = 0;
    while (cursor < text.len)
    {
        U32 cp = 0;
        Memmy_Status status = Memmy_Utf8_Next(text, &cursor, &cp, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        units += cp > 0xffff ? 2 : 1;
    }
    U8 *bytes = Arena_PushArray(arena, U8, units * 2);
    U64 at = 0;
    cursor = 0;
    while (cursor < text.len)
    {
        U32 cp = 0;
        Memmy_Utf8_Next(text, &cursor, &cp, 0);
        if (cp > 0xffff)
        {
            cp -= 0x10000;
            U32 high = 0xd800 + (cp >> 10);
            U32 low = 0xdc00 + (cp & 0x3ff);
            bytes[at++] = (U8)high;
            bytes[at++] = (U8)(high >> 8);
            bytes[at++] = (U8)low;
            bytes[at++] = (U8)(low >> 8);
        }
        else
        {
            bytes[at++] = (U8)cp;
            bytes[at++] = (U8)(cp >> 8);
        }
    }
    *out = String8_Make(bytes, units * 2);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Value_Encode(Arena *arena, Memmy_Value const *value, Memmy_EncodedValue *out, Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_EncodedValue){0};
    }
    if (arena == 0 || value == 0 || out == 0 || !Memmy_Type_IsValid(value->type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid encode arguments"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_EncodedValue encoded = {.type = value->type};
    if (Memmy_Type_IsInteger(value->type) || Memmy_Type_IsFloat(value->type))
    {
        U64 size = Memmy_Type_EncodedSize(value->type);
        U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
        U64 raw = Memmy_Type_IsFloat(value->type) ? value->floating_bits
                  : value->type.integer.is_signed ? (U64)value->signed_integer
                                                  : value->unsigned_integer;
        Memmy_Integer_WriteLe(bytes, size, raw);
        encoded.bytes = String8_Make(bytes, size);
    }
    else if (Memmy_Type_IsString(value->type))
    {
        Memmy_Status status = Memmy_Utf8_Validate(value->string, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (value->type.string.encoding == Memmy_StringEncoding_Utf8)
        {
            U64 terminator = value->type.string.zero_terminated ? 1 : 0;
            U8 *bytes = Arena_PushArray(arena, U8, value->string.len + terminator);
            Memory_Copy(bytes, value->string.data, value->string.len);
            encoded.bytes = String8_Make(bytes, value->string.len + terminator);
        }
        else
        {
            status =
                Memmy_Utf16Le_Encode(arena, value->string, value->type.string.zero_terminated, &encoded.bytes, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
    }
    else
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("type is not encodable"));
        return Memmy_Status_InvalidArgument;
    }
    *out = encoded;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

static U32 Memmy_HexDigit_Value(U8 c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10u + c - 'a';
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10u + c - 'A';
    }
    return U32_MAX;
}

static Memmy_Status Memmy_Integer_Parse(String8 text, B32 is_signed, U64 *unsigned_out, I64 *signed_out)
{
    if (text.len == 0)
    {
        return Memmy_Status_ParseError;
    }
    B32 negative = 0;
    U64 at = 0;
    if (text.data[at] == '-' || text.data[at] == '+')
    {
        negative = text.data[at] == '-';
        at++;
    }
    if ((!is_signed && (negative || at != 0)) || at == text.len)
    {
        return Memmy_Status_ParseError;
    }
    U32 base = 10;
    if (at + 2 <= text.len && text.data[at] == '0' && (text.data[at + 1] == 'x' || text.data[at + 1] == 'X'))
    {
        base = 16;
        at += 2;
    }
    if (at == text.len)
    {
        return Memmy_Status_ParseError;
    }
    U64 magnitude = 0;
    for (; at < text.len; at++)
    {
        U32 digit = base == 16                     ? Memmy_HexDigit_Value(text.data[at])
                    : Char8_IsDigit(text.data[at]) ? text.data[at] - '0'
                                                   : U32_MAX;
        if (digit >= base)
        {
            return Memmy_Status_ParseError;
        }
        if (magnitude > (U64_MAX - digit) / base)
        {
            return Memmy_Status_Overflow;
        }
        magnitude = magnitude * base + digit;
    }
    if (is_signed)
    {
        U64 limit = negative ? (U64)I64_MAX + 1 : (U64)I64_MAX;
        if (magnitude > limit)
        {
            return Memmy_Status_Overflow;
        }
        *signed_out = negative ? magnitude == limit ? I64_MIN : -(I64)magnitude : (I64)magnitude;
    }
    else
    {
        *unsigned_out = magnitude;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_EncodedValue_Parse(Arena *arena, Memmy_Type type, String8 text, Memmy_EncodedValue *out,
                                      Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_EncodedValue){0};
    }
    if (arena == 0 || out == 0 || !Memmy_Type_IsValid(type))
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("invalid parse arguments"));
        return Memmy_Status_InvalidArgument;
    }
    Memmy_Value value = {.type = type};
    Memmy_Status status = Memmy_Status_Ok;
    if (Memmy_Type_IsInteger(type))
    {
        status = Memmy_Integer_Parse(text, type.integer.is_signed, &value.unsigned_integer, &value.signed_integer);
        if (status == Memmy_Status_Ok)
        {
            Memmy_Value converted = {0};
            Memmy_Type source_type = type.integer.is_signed ? Memmy_Type_I64 : Memmy_Type_U64;
            value.type = source_type;
            status = Memmy_Value_Convert(arena, &value, type, &converted, error);
            value = converted;
        }
    }
    else if (Memmy_Type_IsFloat(type))
    {
        F64 number = 0;
        U64 offset = 0;
        if (String8_ParseF64(text, &number, &offset) != String8_ParseStatus_Ok)
        {
            status = Memmy_Status_ParseError;
        }
        else
        {
            value.floating_bits = Memmy_Value_FloatBitsFromF64(number, type.floating.bit_count);
        }
    }
    else if (Memmy_Type_IsString(type))
    {
        value.string = text;
    }
    else
    {
        status = Memmy_Status_InvalidArgument;
    }
    if (status != Memmy_Status_Ok)
    {
        if (error == 0 || error->status == 0)
        {
            Memmy_Value_ErrorInput(error, status, String8_Lit("invalid encoded value"), text, 0, text.len);
        }
        return status;
    }
    return Memmy_Value_Encode(arena, &value, out, error);
}

static B32 Memmy_Char_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

Memmy_Status Memmy_Pattern_Parse(Arena *arena, String8 text, Memmy_PatternParseFlags flags, Memmy_Pattern *out,
                                 Memmy_Error *error)
{
    if (out != 0)
    {
        *out = (Memmy_Pattern){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_Value_Error(error, Memmy_Status_InvalidArgument, String8_Lit("missing arena or output pattern"));
        return Memmy_Status_InvalidArgument;
    }
    U64 count = 0;
    U64 cursor = 0;
    while (cursor < text.len)
    {
        while (cursor < text.len && Memmy_Char_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        if (cursor == text.len)
        {
            break;
        }
        count++;
        while (cursor < text.len && !Memmy_Char_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
    }
    if (count == 0)
    {
        Memmy_Value_ErrorInput(error, Memmy_Status_ParseError, String8_Lit("expected pattern"), text, 0, 0);
        return Memmy_Status_ParseError;
    }
    Memmy_PatternByte *bytes = Arena_PushArray(arena, Memmy_PatternByte, count);
    cursor = 0;
    U64 index = 0;
    while (cursor < text.len)
    {
        while (cursor < text.len && Memmy_Char_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        if (cursor == text.len)
        {
            break;
        }
        U64 start = cursor;
        while (cursor < text.len && !Memmy_Char_IsWhitespace(text.data[cursor]))
        {
            cursor++;
        }
        String8 token = String8_Substr(text, start, cursor - start);
        if (String8_Eq(token, String8_Lit("?")) || String8_Eq(token, String8_Lit("??")))
        {
            if ((flags & Memmy_PatternParseFlag_AllowWildcards) == 0)
            {
                Memmy_Value_ErrorInput(error, Memmy_Status_ParseError, String8_Lit("wildcards are not allowed"), text,
                                       start, token.len);
                return Memmy_Status_ParseError;
            }
            bytes[index++].wildcard = 1;
        }
        else
        {
            U32 hi = token.len == 2 ? Memmy_HexDigit_Value(token.data[0]) : U32_MAX;
            U32 lo = token.len == 2 ? Memmy_HexDigit_Value(token.data[1]) : U32_MAX;
            if (hi >= 16 || lo >= 16)
            {
                Memmy_Value_ErrorInput(error, Memmy_Status_ParseError, String8_Lit("invalid byte token"), text, start,
                                       token.len);
                return Memmy_Status_ParseError;
            }
            bytes[index++].value = (U8)((hi << 4) | lo);
        }
    }
    *out = (Memmy_Pattern){.bytes = bytes, .count = count};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
