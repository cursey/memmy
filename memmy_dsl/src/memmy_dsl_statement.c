#include "memmy_dsl.h"

typedef struct Memmy_StatementSlice Memmy_StatementSlice;
struct Memmy_StatementSlice
{
    String8 text;
    U64 offset;
};

static void Memmy_StatementError_SetInput(Memmy_Error *error, Memmy_Status status, String8 context, String8 message,
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

static B32 Memmy_Statement_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static Memmy_StatementSlice Memmy_Statement_TrimSlice(String8 text, U64 offset, U64 len)
{
    U64 start = offset;
    U64 end = offset + len;
    while (start < end && Memmy_Statement_IsWhitespace(text.data[start]))
    {
        start++;
    }
    while (end > start && Memmy_Statement_IsWhitespace(text.data[end - 1]))
    {
        end--;
    }
    return (Memmy_StatementSlice){.text = String8_Substr(text, start, end - start), .offset = start};
}

static B32 Memmy_VariableRef_IsIdentStart(U8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static B32 Memmy_VariableRef_IsIdentContinue(U8 c)
{
    return Memmy_VariableRef_IsIdentStart(c) || Char8_IsDigit(c);
}

static Memmy_Status Memmy_Statement_ParseVariable(String8 input, Memmy_StatementSlice slice, Memmy_VariableRef *out,
                                                  Memmy_Error *error)
{
    if (slice.text.len < 2 || slice.text.data[0] != '$' || !Memmy_VariableRef_IsIdentStart(slice.text.data[1]))
    {
        Memmy_StatementError_SetInput(error, Memmy_Status_ParseError, String8_Lit("statement"),
                                      String8_Lit("invalid variable name"), input, slice.offset, slice.text.len);
        return Memmy_Status_ParseError;
    }

    for (U64 i = 2; i < slice.text.len; i++)
    {
        if (!Memmy_VariableRef_IsIdentContinue(slice.text.data[i]))
        {
            Memmy_StatementError_SetInput(error, Memmy_Status_ParseError, String8_Lit("statement"),
                                          String8_Lit("invalid variable name"), input, slice.offset + i, 1);
            return Memmy_Status_ParseError;
        }
    }

    *out = (Memmy_VariableRef){
        .name = String8_Substr(slice.text, 1, slice.text.len - 1),
    };
    return Memmy_Status_Ok;
}

static void Memmy_Statement_RemapError(Memmy_Error *error, String8 input, U64 base_offset)
{
    if (error != 0)
    {
        error->input = input;
        error->byte_offset += base_offset;
    }
}

static B32 Memmy_Statement_TopLevelFindChar(String8 text, U8 needle, U64 *out)
{
    B32 result = 0;
    U64 paren_depth = 0;
    U64 bracket_depth = 0;
    B32 in_target = 0;
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        if (in_target)
        {
            if (c == '>')
            {
                in_target = 0;
            }
            continue;
        }
        if (c == '<')
        {
            in_target = 1;
            continue;
        }
        if (c == '(')
        {
            paren_depth++;
            continue;
        }
        if (c == ')' && paren_depth > 0)
        {
            paren_depth--;
            continue;
        }
        if (c == '[')
        {
            bracket_depth++;
            continue;
        }
        if (c == ']' && bracket_depth > 0)
        {
            bracket_depth--;
            continue;
        }
        if (paren_depth == 0 && bracket_depth == 0 && c == needle)
        {
            *out = i;
            result = 1;
            break;
        }
    }
    return result;
}

static B32 Memmy_Statement_IsWrappedInTopLevelParens(String8 text)
{
    B32 result = 0;
    if (text.len >= 2 && text.data[0] == '(' && text.data[text.len - 1] == ')')
    {
        U64 depth = 0;
        result = 1;
        for (U64 i = 0; i < text.len; i++)
        {
            if (text.data[i] == '(')
            {
                depth++;
            }
            else if (text.data[i] == ')')
            {
                if (depth == 0)
                {
                    result = 0;
                    break;
                }
                depth--;
                if (depth == 0 && i + 1 < text.len)
                {
                    result = 0;
                    break;
                }
            }
        }
        result = result && depth == 0;
    }
    return result;
}

/*
variable_expr = range_expr | address_expr | "(", const_expr, ")"

Parentheses force const_expr parsing. Otherwise address_expr is tried first,
then range_expr.
*/
Memmy_Status Memmy_VariableExpr_Parse(Arena *arena, String8 text, Memmy_VariableExpr *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                        String8_Lit("missing arena or output variable expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_StatementSlice source = Memmy_Statement_TrimSlice(text, 0, text.len);
    if (source.text.len == 0)
    {
        Memmy_StatementError_SetInput(error, Memmy_Status_ParseError, String8_Lit("statement"),
                                      String8_Lit("expected variable expression"), text, source.offset, 0);
        return Memmy_Status_ParseError;
    }

    if (Memmy_Statement_IsWrappedInTopLevelParens(source.text))
    {
        Memmy_ConstExpr constant = {0};
        Memmy_Status status =
            Memmy_ConstExpr_Parse(arena, String8_Substr(source.text, 1, source.text.len - 2), &constant, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Statement_RemapError(error, text, source.offset + 1);
            return status;
        }
        *out = (Memmy_VariableExpr){
            .kind = Memmy_VariableExprKind_Const,
            .constant = constant,
        };
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    Memmy_AddressExpr address = {0};
    Memmy_Status status = Memmy_AddressExpr_Parse(arena, source.text, &address, 0);
    if (status == Memmy_Status_Ok)
    {
        *out = (Memmy_VariableExpr){
            .kind = Memmy_VariableExprKind_Address,
            .address = address,
        };
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    Memmy_RangeExpr range = {0};
    status = Memmy_RangeExpr_Parse(arena, source.text, &range, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Statement_RemapError(error, text, source.offset);
        return status;
    }
    *out = (Memmy_VariableExpr){
        .kind = Memmy_VariableExprKind_Range,
        .range = range,
    };
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

/*
statement       = meta_stmt | assignment_stmt | memory_expr
meta_stmt       = "procs", [ ws, procs_filter ]
                | "vars"
                | "unset", ws, variable
                | "exit"
                | "quit"
assignment_stmt = variable, ws_opt, "=", ws_opt, variable_expr

Top-level statement input is trimmed before parsing. A single "=" is assignment
only when the statement starts with a variable; "==" remains part of memory_expr
value scans.
*/
Memmy_Status Memmy_Statement_Parse(Arena *arena, String8 text, Memmy_Statement *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                        String8_Lit("missing arena or output statement"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_StatementSlice source = Memmy_Statement_TrimSlice(text, 0, text.len);
    if (source.text.len == 0)
    {
        Memmy_StatementError_SetInput(error, Memmy_Status_ParseError, String8_Lit("statement"),
                                      String8_Lit("expected statement"), text, source.offset, 0);
        return Memmy_Status_ParseError;
    }

    if (String8_Eq(source.text, String8_Lit("procs")))
    {
        *out = (Memmy_Statement){.kind = Memmy_StatementKind_Procs};
    }
    else if (source.text.len > 5 && String8_Eq(String8_Substr(source.text, 0, 5), String8_Lit("procs")) &&
             Memmy_Statement_IsWhitespace(source.text.data[5]))
    {
        Memmy_StatementSlice filter = Memmy_Statement_TrimSlice(text, source.offset + 5, source.text.len - 5);
        if (filter.text.len == 0)
        {
            *out = (Memmy_Statement){.kind = Memmy_StatementKind_Procs};
        }
        else
        {
            *out = (Memmy_Statement){
                .kind = Memmy_StatementKind_Procs,
                .procs_filter = filter.text,
            };
        }
    }
    else if (String8_Eq(source.text, String8_Lit("vars")))
    {
        *out = (Memmy_Statement){.kind = Memmy_StatementKind_Vars};
    }
    else if (String8_Eq(source.text, String8_Lit("exit")) || String8_Eq(source.text, String8_Lit("quit")))
    {
        *out = (Memmy_Statement){.kind = Memmy_StatementKind_Exit};
    }
    else if (source.text.len > 5 && String8_Eq(String8_Substr(source.text, 0, 5), String8_Lit("unset")) &&
             Memmy_Statement_IsWhitespace(source.text.data[5]))
    {
        Memmy_StatementSlice variable = Memmy_Statement_TrimSlice(text, source.offset + 5, source.text.len - 5);
        Memmy_VariableRef variable_ref = {0};
        Memmy_Status status = Memmy_Statement_ParseVariable(text, variable, &variable_ref, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_Statement){
            .kind = Memmy_StatementKind_Unset,
            .variable = variable_ref,
        };
    }
    else
    {
        U64 equal = 0;
        if (source.text.data[0] == '$' && Memmy_Statement_TopLevelFindChar(source.text, '=', &equal) &&
            (equal + 1 >= source.text.len || source.text.data[equal + 1] != '='))
        {
            Memmy_StatementSlice variable = Memmy_Statement_TrimSlice(text, source.offset, equal);
            Memmy_StatementSlice rhs =
                Memmy_Statement_TrimSlice(text, source.offset + equal + 1, source.text.len - equal - 1);
            if (rhs.text.len == 0)
            {
                Memmy_StatementError_SetInput(error, Memmy_Status_ParseError, String8_Lit("statement"),
                                              String8_Lit("expected assignment expression"), text, rhs.offset, 0);
                return Memmy_Status_ParseError;
            }

            Memmy_VariableRef variable_ref = {0};
            Memmy_Status status = Memmy_Statement_ParseVariable(text, variable, &variable_ref, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }

            Memmy_VariableExpr variable_expr = {0};
            status = Memmy_VariableExpr_Parse(arena, rhs.text, &variable_expr, error);
            if (status != Memmy_Status_Ok)
            {
                Memmy_Statement_RemapError(error, text, rhs.offset);
                return status;
            }

            *out = (Memmy_Statement){
                .kind = Memmy_StatementKind_Assignment,
                .variable = variable_ref,
                .assignment = variable_expr,
            };
        }
        else
        {
            Memmy_MemoryExpr memory = {0};
            Memmy_Status status = Memmy_MemoryExpr_Parse(arena, source.text, &memory, error);
            if (status != Memmy_Status_Ok)
            {
                Memmy_Statement_RemapError(error, text, source.offset);
                return status;
            }
            *out = (Memmy_Statement){
                .kind = Memmy_StatementKind_Memory,
                .memory = memory,
            };
        }
    }

    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
