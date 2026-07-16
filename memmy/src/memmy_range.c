#include "memmy_range.h"

#include "base.h"

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

static U32 Memmy_HexDigit_Value(U8 c)
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

static Memmy_Status Memmy_UnsignedToken_Parse(String8 text, String8 context, U64 *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, context, String8_Lit("missing output pointer"));
        return Memmy_Status_InvalidArgument;
    }

    if (text.len == 0)
    {
        Memmy_Error_SetInput(error, Memmy_Status_ParseError, context, String8_Lit("expected unsigned integer"), text, 0,
                             0);
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
            Memmy_Error_SetInput(error, Memmy_Status_ParseError, context, String8_Lit("expected hexadecimal digit"),
                                 text, 2, 1);
            return Memmy_Status_ParseError;
        }
    }

    U64 value = 0;
    for (U64 i = first_digit; i < text.len; i++)
    {
        U8 c = text.data[i];
        U32 digit = (base == 16) ? Memmy_HexDigit_Value(c) : (Char8_IsDigit(c) ? (U32)(c - '0') : U32_MAX);
        if (digit >= base)
        {
            Memmy_Error_SetInput(error, Memmy_Status_ParseError, context, String8_Lit("invalid unsigned integer"), text,
                                 i, 1);
            return Memmy_Status_ParseError;
        }

        if (value > (U64_MAX - digit) / base)
        {
            Memmy_Error_SetInput(error, Memmy_Status_Overflow, context, String8_Lit("unsigned integer overflow"), text,
                                 i, 1);
            return Memmy_Status_Overflow;
        }
        value = value * base + digit;
    }

    *out = value;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Address_Parse(String8 text, Memmy_Addr *out, Memmy_Error *error)
{
    return Memmy_UnsignedToken_Parse(text, String8_Lit("address"), out, error);
}

Memmy_Status Memmy_Size_Parse(String8 text, Memmy_Size *out, Memmy_Error *error)
{
    return Memmy_UnsignedToken_Parse(text, String8_Lit("range"), out, error);
}

Memmy_Status Memmy_Range_FromStartEnd(Memmy_Addr start, Memmy_Addr end, Memmy_Range *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"), String8_Lit("missing output range"));
        return Memmy_Status_InvalidArgument;
    }
    if (end < start)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                        String8_Lit("range end is before start"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (Memmy_Range){.start = start, .end = end};
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Range_FromStartLength(Memmy_Addr start, Memmy_Size length, Memmy_Range *out, Memmy_Error *error)
{
    Memmy_Addr end = 0;
    if (!AddU64Checked(start, length, &end))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("range"), String8_Lit("range end overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Range_FromStartEnd(start, end, out, error);
}
