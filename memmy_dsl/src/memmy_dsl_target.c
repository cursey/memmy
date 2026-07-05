#include "memmy_dsl.h"

static void Memmy_ExprError_SetInput(Memmy_Error *error, Memmy_Status status, String8 context, String8 message,
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

static B32 Memmy_TargetPart_HasBoundaryWhitespace(String8 text)
{
    B32 result = 0;
    if (text.len > 0)
    {
        U8 first = text.data[0];
        U8 last = text.data[text.len - 1];
        result = (first == ' ' || first == '\t' || first == '\n' || first == '\r' || last == ' ' || last == '\t' ||
                  last == '\n' || last == '\r');
    }
    return result;
}

static B32 Memmy_TargetPart_HasInvalidNameChar(String8 text)
{
    B32 result = 0;
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        if (c == '<' || c == '>' || c == '!')
        {
            result = 1;
            break;
        }
    }
    return result;
}

static Memmy_Status Memmy_TargetExpr_ParsePid(String8 text, U32 *out, Memmy_Error *error, String8 input,
                                              U64 input_offset)
{
    U64 value = 0;
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        if (!Char8_IsDigit(c))
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                            String8_Lit("pid selector contains non-digit"));
            return Memmy_Status_InvalidArgument;
        }
        U32 digit = (U32)(c - '0');
        if (value > ((U64)U32_MAX - digit) / 10u)
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_Overflow, String8_Lit("target"),
                                     String8_Lit("pid selector overflow"), input, input_offset + i, 1);
            return Memmy_Status_Overflow;
        }
        value = value * 10u + digit;
    }
    *out = (U32)value;
    return Memmy_Status_Ok;
}

static B32 Memmy_TargetPart_IsDecimal(String8 text)
{
    B32 result = text.len > 0;
    for (U64 i = 0; i < text.len; i++)
    {
        if (!Char8_IsDigit(text.data[i]))
        {
            result = 0;
            break;
        }
    }
    return result;
}

static Memmy_Status Memmy_TargetExpr_ValidateNamePart(String8 input, String8 part, U64 input_offset, Memmy_Error *error)
{
    if (part.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("target"),
                                 String8_Lit("target part cannot be empty"), input, input_offset, 0);
        return Memmy_Status_ParseError;
    }
    if (Memmy_TargetPart_HasBoundaryWhitespace(part))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("target"),
                                 String8_Lit("target part has leading or trailing whitespace"), input, input_offset,
                                 part.len);
        return Memmy_Status_ParseError;
    }
    if (Memmy_TargetPart_HasInvalidNameChar(part))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("target"),
                                 String8_Lit("target name contains invalid character"), input, input_offset, part.len);
        return Memmy_Status_ParseError;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_TargetExpr_Parse(String8 text, Memmy_TargetExpr *out, Memmy_Error *error)
{
    if (out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing output target"));
        return Memmy_Status_InvalidArgument;
    }
    if (text.len < 3 || text.data[0] != '<' || text.data[text.len - 1] != '>')
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("target"),
                                 String8_Lit("expected target ref"), text, 0, text.len);
        return Memmy_Status_ParseError;
    }

    String8 body = String8_Substr(text, 1, text.len - 2);
    U64 bang = String8_FindChar(body, '!', 0);
    Memmy_TargetExpr result = {
        .kind = Memmy_TargetExprKind_Module,
    };

    if (bang == STRING8_NPOS)
    {
        Memmy_Status status = Memmy_TargetExpr_ValidateNamePart(text, body, 1, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        result.module_name = body;
    }
    else
    {
        String8 selector = String8_Substr(body, 0, bang);
        String8 module = String8_Substr(body, bang + 1, body.len - bang - 1);

        Memmy_Status status = Memmy_TargetExpr_ValidateNamePart(text, selector, 1, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        if (Memmy_TargetPart_IsDecimal(selector))
        {
            result.process.kind = Memmy_ProcessSelectorKind_Pid;
            status = Memmy_TargetExpr_ParsePid(selector, &result.process.pid, error, text, 1);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        else
        {
            result.process.kind = Memmy_ProcessSelectorKind_Name;
            result.process.name = selector;
        }

        if (module.len == 0)
        {
            result.kind = Memmy_TargetExprKind_WholeProcess;
        }
        else
        {
            status = Memmy_TargetExpr_ValidateNamePart(text, module, 1 + bang + 1, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            result.module_name = module;
        }
    }

    *out = result;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
