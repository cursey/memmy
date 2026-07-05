#include "memmy_dsl.h"

typedef struct Memmy_ExprSlice Memmy_ExprSlice;
struct Memmy_ExprSlice
{
    String8 text;
    U64 offset;
};

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

static B32 Memmy_Expr_IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static B32 Memmy_VariableRef_IsIdentStart(U8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static B32 Memmy_VariableRef_IsIdentContinue(U8 c)
{
    return Memmy_VariableRef_IsIdentStart(c) || Char8_IsDigit(c);
}

static Memmy_Status Memmy_Expr_ParseVariable(String8 text, Memmy_VariableRef *out, Memmy_Error *error)
{
    if (text.len < 2 || text.data[0] != '$' || !Memmy_VariableRef_IsIdentStart(text.data[1]))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("invalid variable name"), text, 0, text.len);
        return Memmy_Status_ParseError;
    }

    for (U64 i = 2; i < text.len; i++)
    {
        if (!Memmy_VariableRef_IsIdentContinue(text.data[i]))
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("invalid variable name"), text, i, 1);
            return Memmy_Status_ParseError;
        }
    }

    *out = (Memmy_VariableRef){
        .name = String8_Substr(text, 1, text.len - 1),
    };
    return Memmy_Status_Ok;
}

static Memmy_ExprSlice Memmy_Expr_TrimSlice(String8 text, U64 offset, U64 len)
{
    U64 start = offset;
    U64 end = offset + len;
    while (start < end && Memmy_Expr_IsWhitespace(text.data[start]))
    {
        start++;
    }
    while (end > start && Memmy_Expr_IsWhitespace(text.data[end - 1]))
    {
        end--;
    }
    return (Memmy_ExprSlice){.text = String8_Substr(text, start, end - start), .offset = start};
}

static Memmy_ExprSlice Memmy_Expr_RawSlice(String8 text, U64 offset, U64 len)
{
    return (Memmy_ExprSlice){.text = String8_Substr(text, offset, len), .offset = offset};
}

static void Memmy_Expr_RemapError(Memmy_Error *error, String8 full_input, U64 base_offset)
{
    if (error != 0)
    {
        error->input = full_input;
        error->byte_offset += base_offset;
    }
}

static Memmy_Status Memmy_Expr_RejectBoundaryWhitespace(String8 full_input, Memmy_ExprSlice slice, String8 context,
                                                        Memmy_Error *error)
{
    if (slice.text.len > 0 && Memmy_Expr_IsWhitespace(slice.text.data[0]))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, context,
                                 String8_Lit("unexpected whitespace in range expression"), full_input, slice.offset, 1);
        return Memmy_Status_ParseError;
    }
    if (slice.text.len > 0 && Memmy_Expr_IsWhitespace(slice.text.data[slice.text.len - 1]))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, context,
                                 String8_Lit("unexpected whitespace in range expression"), full_input,
                                 slice.offset + slice.text.len - 1, 1);
        return Memmy_Status_ParseError;
    }
    return Memmy_Status_Ok;
}

static B32 Memmy_Expr_TopLevelFind(String8 text, String8 needle, U64 *out)
{
    B32 result = 0;
    U64 paren_depth = 0;
    U64 bracket_depth = 0;
    B32 in_target = 0;
    for (U64 i = 0; i + needle.len <= text.len; i++)
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
        if (paren_depth == 0 && bracket_depth == 0 && String8_Eq(String8_Substr(text, i, needle.len), needle))
        {
            *out = i;
            result = 1;
            break;
        }
    }
    return result;
}

static B32 Memmy_Expr_TopLevelColon(String8 text, U64 *out)
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
        if (paren_depth == 0 && bracket_depth == 0 && c == ':' && (i + 1 >= text.len || text.data[i + 1] != '+'))
        {
            *out = i;
            result = 1;
            break;
        }
    }
    return result;
}

static Memmy_Status Memmy_Expr_ParseTargetAtStart(String8 text, Memmy_TargetExpr *out, U64 *out_close,
                                                  Memmy_Error *error)
{
    if (text.len == 0 || text.data[0] != '<')
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected target ref"), text, 0, 0);
        return Memmy_Status_ParseError;
    }

    U64 close = String8_FindChar(text, '>', 0);
    if (close == STRING8_NPOS)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected target ref"), text, 0, text.len);
        return Memmy_Status_ParseError;
    }

    String8 target_text = String8_Substr(text, 0, close + 1);
    Memmy_Status status = Memmy_TargetExpr_Parse(target_text, out, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out_close = close;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Expr_ParseConstExprAt(Arena *arena, String8 full_input, Memmy_ExprSlice slice,
                                                Memmy_ConstExpr *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Expr_RejectBoundaryWhitespace(full_input, slice, String8_Lit("range"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ConstExpr const_expr = {0};
    status = Memmy_ConstExpr_Parse(arena, slice.text, &const_expr, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, full_input, slice.offset);
        return status;
    }
    *out = const_expr;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Expr_ParseConstAt(Arena *arena, String8 full_input, Memmy_ExprSlice slice, I64 *out,
                                            Memmy_ConstExpr *out_expr, Memmy_Error *error)
{
    Memmy_ConstExpr const_expr = {0};
    Memmy_Status status = Memmy_Expr_ParseConstExprAt(arena, full_input, slice, &const_expr, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = const_expr.value;
    if (out_expr != 0)
    {
        *out_expr = const_expr;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Expr_ParseSizeAt(Arena *arena, String8 full_input, Memmy_ExprSlice slice, Memmy_Size *out,
                                           Memmy_ConstExpr *out_expr, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Expr_RejectBoundaryWhitespace(full_input, slice, String8_Lit("range"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (slice.text.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"), String8_Lit("expected size"),
                                 full_input, slice.offset, 0);
        return Memmy_Status_ParseError;
    }

    if (slice.text.data[0] == '(')
    {
        if (slice.text.len < 2 || slice.text.data[slice.text.len - 1] != ')')
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"), String8_Lit("expected ')'"),
                                     full_input, slice.offset + slice.text.len, 0);
            return Memmy_Status_ParseError;
        }
        Memmy_ExprSlice inner = Memmy_Expr_TrimSlice(full_input, slice.offset + 1, slice.text.len - 2);
        I64 value = 0;
        Memmy_ConstExpr expr = {0};
        status = Memmy_Expr_ParseConstAt(arena, full_input, inner, &value, &expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (!expr.contains_variable && value < 0)
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                                     String8_Lit("size cannot be negative"), full_input, inner.offset, inner.text.len);
            return Memmy_Status_ParseError;
        }
        *out = (Memmy_Size)value;
        if (out_expr != 0)
        {
            *out_expr = expr;
        }
        return Memmy_Status_Ok;
    }

    if (slice.text.data[0] == '$')
    {
        Memmy_ConstExpr expr = {0};
        status = Memmy_Expr_ParseConstExprAt(arena, full_input, slice, &expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_Size)expr.value;
        if (out_expr != 0)
        {
            *out_expr = expr;
        }
        return Memmy_Status_Ok;
    }

    status = Memmy_ParseSize(slice.text, out, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, full_input, slice.offset);
        return status;
    }
    if (out_expr != 0)
    {
        *out_expr = (Memmy_ConstExpr){
            .kind = Memmy_ConstExprKind_Literal,
            .value = (I64)*out,
        };
    }
    return status;
}

static Memmy_Status Memmy_RangeExpr_ParseModuleBracket(Arena *arena, String8 text, Memmy_TargetExpr target,
                                                       U64 bracket_offset, Memmy_RangeExpr *out, Memmy_Error *error)
{
    (void)arena;
    if (text.len == 0 || text.data[text.len - 1] != ']')
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"), String8_Lit("expected ']'"),
                                 text, text.len, 0);
        return Memmy_Status_ParseError;
    }

    U64 inner_offset = bracket_offset + 1;
    String8 inner = String8_Substr(text, inner_offset, text.len - inner_offset - 1);
    U64 dots = String8_Find(inner, String8_Lit(".."), 0);
    U64 sized = String8_Find(inner, String8_Lit(":+"), 0);
    if (dots != STRING8_NPOS)
    {
        Memmy_ExprSlice start = Memmy_Expr_RawSlice(text, inner_offset, dots);
        Memmy_ExprSlice end = Memmy_Expr_RawSlice(text, inner_offset + dots + 2, inner.len - dots - 2);
        I64 start_offset = 0;
        I64 end_offset = 0;
        Memmy_ConstExpr start_expr = {0};
        Memmy_ConstExpr end_expr = {0};
        Memmy_Status status = Memmy_Expr_ParseConstAt(arena, text, start, &start_offset, &start_expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Expr_ParseConstAt(arena, text, end, &end_offset, &end_expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_RangeExpr){
            .kind = Memmy_RangeExprKind_TargetOffset,
            .target = target,
            .start_offset = start_offset,
            .end_offset = end_offset,
            .start_offset_expr = start_expr,
            .end_offset_expr = end_expr,
        };
        return Memmy_Status_Ok;
    }
    if (sized != STRING8_NPOS)
    {
        Memmy_ExprSlice start = Memmy_Expr_RawSlice(text, inner_offset, sized);
        Memmy_ExprSlice size = Memmy_Expr_RawSlice(text, inner_offset + sized + 2, inner.len - sized - 2);
        I64 start_offset = 0;
        I64 size_value = 0;
        Memmy_ConstExpr start_expr = {0};
        Memmy_ConstExpr size_expr = {0};
        Memmy_Status status = Memmy_Expr_ParseConstAt(arena, text, start, &start_offset, &start_expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Expr_ParseConstAt(arena, text, size, &size_value, &size_expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (!size_expr.contains_variable && size_value < 0)
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                                     String8_Lit("size cannot be negative"), text, size.offset, size.text.len);
            return Memmy_Status_ParseError;
        }
        *out = (Memmy_RangeExpr){
            .kind = Memmy_RangeExprKind_TargetSized,
            .target = target,
            .start_offset = start_offset,
            .size = (Memmy_Size)size_value,
            .start_offset_expr = start_expr,
            .size_expr = size_expr,
        };
        return Memmy_Status_Ok;
    }

    Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                             String8_Lit("expected module range operator"), text, inner_offset, inner.len);
    return Memmy_Status_ParseError;
}

/*
range_expr          = target_offset_range
                    | target_sized_range
                    | address_sized_range
                    | target_ref
                    | variable
target_offset_range = target_ref, "[", const_expr, "..", const_expr, "]"
target_sized_range  = target_ref, "[", const_expr, ":+", const_expr, "]"
address_sized_range = address_expr, ":+", size
size                = integer | variable | "(", const_expr, ")"

For module targets, bracket ranges are module-relative offsets. For
whole-process targets, bracket ranges are absolute addresses. Range boundaries
reject whitespace. Address ".." address ranges are intentionally unsupported;
use ":+ size".
*/
Memmy_Status Memmy_RangeExpr_Parse(Arena *arena, String8 text, Memmy_RangeExpr *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                        String8_Lit("missing arena or output range expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExprSlice source = Memmy_Expr_TrimSlice(text, 0, text.len);
    if (source.text.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                                 String8_Lit("expected range expression"), text, source.offset, 0);
        return Memmy_Status_ParseError;
    }

    U64 sized = 0;
    if (Memmy_Expr_TopLevelFind(source.text, String8_Lit(":+"), &sized))
    {
        Memmy_ExprSlice address = Memmy_Expr_RawSlice(text, source.offset, sized);
        Memmy_ExprSlice size = Memmy_Expr_RawSlice(text, source.offset + sized + 2, source.text.len - sized - 2);
        Memmy_AddressExpr address_expr = {0};
        Memmy_Status status = Memmy_Expr_RejectBoundaryWhitespace(text, address, String8_Lit("range"), error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        status = Memmy_AddressExpr_Parse(arena, address.text, &address_expr, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, address.offset);
            return status;
        }

        Memmy_Size size_value = 0;
        Memmy_ConstExpr size_expr = {0};
        status = Memmy_Expr_ParseSizeAt(arena, text, size, &size_value, &size_expr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_RangeExpr){
            .kind = Memmy_RangeExprKind_AddressSized,
            .address = address_expr,
            .size = size_value,
            .size_expr = size_expr,
        };
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    if (source.text.data[0] == '$')
    {
        Memmy_VariableRef variable = {0};
        Memmy_Status status = Memmy_Expr_ParseVariable(source.text, &variable, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, source.offset);
            return status;
        }
        *out = (Memmy_RangeExpr){
            .kind = Memmy_RangeExprKind_Variable,
            .variable = variable,
        };
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    if (source.text.data[0] == '<')
    {
        Memmy_TargetExpr target = {0};
        U64 close = 0;
        Memmy_Status status = Memmy_Expr_ParseTargetAtStart(source.text, &target, &close, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, source.offset);
            return status;
        }

        if (close + 1 == source.text.len)
        {
            *out = (Memmy_RangeExpr){
                .kind = Memmy_RangeExprKind_Target,
                .target = target,
            };
            if (error != 0)
            {
                *error = (Memmy_Error){0};
            }
            return Memmy_Status_Ok;
        }

        if (source.text.data[close + 1] == '[')
        {
            status = Memmy_RangeExpr_ParseModuleBracket(arena, source.text, target, close + 1, out, error);
            if (status != Memmy_Status_Ok)
            {
                Memmy_Expr_RemapError(error, text, source.offset);
                return status;
            }
            if (error != 0)
            {
                *error = (Memmy_Error){0};
            }
            return Memmy_Status_Ok;
        }
    }

    U64 dots = 0;
    if (Memmy_Expr_TopLevelFind(source.text, String8_Lit(".."), &dots))
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                                 String8_Lit("address ranges are not supported"), text, source.offset + dots, 2);
        return Memmy_Status_ParseError;
    }

    Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("range"),
                             String8_Lit("expected range expression"), text, source.offset, source.text.len);
    return Memmy_Status_ParseError;
}

static Memmy_Status Memmy_MemoryExpr_ParseTypeAt(String8 full_input, Memmy_ExprSlice slice, Memmy_Type *out,
                                                 Memmy_Error *error)
{
    Memmy_Status status = Memmy_Type_Parse(slice.text, out, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, full_input, slice.offset);
    }
    return status;
}

static B32 Memmy_MemoryExpr_RhsLooksAddressExpr(Arena *arena, String8 text)
{
    B32 result = 0;
    Scratch scratch = Scratch_Begin(&arena, 1);
    Memmy_AddressExpr address = {0};
    if (Memmy_AddressExpr_Parse(scratch.arena, text, &address, 0) == Memmy_Status_Ok)
    {
        result = (address.base_kind == Memmy_AddressExprBaseKind_Target ||
                  address.base_kind == Memmy_AddressExprBaseKind_ProcessAbsolute || address.ops.count != 0);
    }
    Scratch_End(scratch);
    return result;
}

static Memmy_Status Memmy_MemoryExpr_ParsePattern(Arena *arena, String8 text, Memmy_ExprSlice source,
                                                  Memmy_MemoryExpr *out, Memmy_Error *error)
{
    U64 open = 0;
    if (!Memmy_Expr_TopLevelFind(source.text, String8_Lit("{"), &open))
    {
        return Memmy_Status_ParseError;
    }

    U64 close = String8_FindLastChar(source.text, '}');
    if (close == STRING8_NPOS || close < open)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"), String8_Lit("expected '}'"), text,
                                 source.offset + open, 1);
        return Memmy_Status_ParseError;
    }
    for (U64 i = close + 1; i < source.text.len; i++)
    {
        if (!Memmy_Expr_IsWhitespace(source.text.data[i]))
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                     String8_Lit("unexpected trailing input"), text, source.offset + i, 1);
            return Memmy_Status_ParseError;
        }
    }

    Memmy_ExprSlice range = Memmy_Expr_TrimSlice(text, source.offset, open);
    Memmy_ExprSlice pattern = Memmy_Expr_TrimSlice(text, source.offset + open + 1, close - open - 1);

    Memmy_RangeExpr range_expr = {0};
    Memmy_Status status = Memmy_RangeExpr_Parse(arena, range.text, &range_expr, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, text, range.offset);
        return status;
    }

    Memmy_Pattern parsed_pattern = {0};
    status = Memmy_Pattern_Parse(arena, pattern.text, Memmy_PatternParseFlag_AllowWildcards, &parsed_pattern, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, text, pattern.offset);
        return status;
    }

    *out = (Memmy_MemoryExpr){
        .kind = Memmy_MemoryExprKind_PatternScan,
        .range = range_expr,
        .pattern_text = pattern.text,
        .pattern = parsed_pattern,
    };
    return Memmy_Status_Ok;
}

/*
typed_expr      = poke_expr | value_scan_expr | peek_expr
peek_expr       = address_expr, ws_opt, ":", ws_opt, type
poke_expr       = address_expr, ws_opt, ":", ws_opt, type,
                  ws_opt, "=", ws_opt, value_text
value_scan_expr = range_expr, ws_opt, ":", ws_opt, type,
                  ws_opt, "==", ws_opt, value_text

Ordering comparisons are not v1 syntax. value_text is a trimmed, non-empty
remainder validated later by Memmy_Value_Parse. Poke rejects RHS text that looks
like a target/address-operation address expression.
*/
static Memmy_Status Memmy_MemoryExpr_ParseTyped(Arena *arena, String8 text, Memmy_ExprSlice source,
                                                Memmy_MemoryExpr *out, Memmy_Error *error)
{
    U64 colon = 0;
    if (!Memmy_Expr_TopLevelColon(source.text, &colon))
    {
        return Memmy_Status_ParseError;
    }

    Memmy_ExprSlice lhs = Memmy_Expr_TrimSlice(text, source.offset, colon);
    Memmy_ExprSlice rhs = Memmy_Expr_TrimSlice(text, source.offset + colon + 1, source.text.len - colon - 1);
    if (lhs.text.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected expression before ':'"), text, lhs.offset, 0);
        return Memmy_Status_ParseError;
    }
    if (rhs.text.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("type"), String8_Lit("expected type"),
                                 text, rhs.offset, 0);
        return Memmy_Status_ParseError;
    }

    U64 type_len = 0;
    while (type_len < rhs.text.len)
    {
        U8 c = rhs.text.data[type_len];
        if (Memmy_Expr_IsWhitespace(c) || c == '=' || c == '<' || c == '>' || c == '!')
        {
            break;
        }
        type_len++;
    }
    Memmy_ExprSlice type_slice = Memmy_Expr_TrimSlice(text, rhs.offset, type_len);
    Memmy_ExprSlice tail = Memmy_Expr_TrimSlice(text, rhs.offset + type_len, rhs.text.len - type_len);
    Memmy_Type type = {0};
    Memmy_Status status = Memmy_MemoryExpr_ParseTypeAt(text, type_slice, &type, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (tail.text.len == 0)
    {
        Memmy_AddressExpr address = {0};
        status = Memmy_AddressExpr_Parse(arena, lhs.text, &address, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, lhs.offset);
            return status;
        }
        *out = (Memmy_MemoryExpr){
            .kind = Memmy_MemoryExprKind_Peek,
            .address = address,
            .type = type,
        };
        return Memmy_Status_Ok;
    }

    if (tail.text.len >= 2 && tail.text.data[0] == '=' && tail.text.data[1] == '=')
    {
        Memmy_ExprSlice value = Memmy_Expr_TrimSlice(text, tail.offset + 2, tail.text.len - 2);
        if (value.text.len == 0)
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("value"),
                                     String8_Lit("expected value"), text, value.offset, 0);
            return Memmy_Status_ParseError;
        }

        Memmy_RangeExpr range = {0};
        status = Memmy_RangeExpr_Parse(arena, lhs.text, &range, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, lhs.offset);
            return status;
        }
        *out = (Memmy_MemoryExpr){
            .kind = Memmy_MemoryExprKind_ValueScan,
            .range = range,
            .type = type,
            .value_text = value.text,
        };
        return Memmy_Status_Ok;
    }

    if (tail.text.data[0] == '=')
    {
        Memmy_ExprSlice value = Memmy_Expr_TrimSlice(text, tail.offset + 1, tail.text.len - 1);
        if (value.text.len == 0)
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("value"),
                                     String8_Lit("expected value"), text, value.offset, 0);
            return Memmy_Status_ParseError;
        }
        if (Memmy_MemoryExpr_RhsLooksAddressExpr(arena, value.text))
        {
            Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("value"),
                                     String8_Lit("rhs address expressions are not supported"), text, value.offset,
                                     value.text.len);
            return Memmy_Status_ParseError;
        }

        Memmy_AddressExpr address = {0};
        status = Memmy_AddressExpr_Parse(arena, lhs.text, &address, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Expr_RemapError(error, text, lhs.offset);
            return status;
        }
        *out = (Memmy_MemoryExpr){
            .kind = Memmy_MemoryExprKind_Poke,
            .address = address,
            .type = type,
            .value_text = value.text,
        };
        return Memmy_Status_Ok;
    }

    if (tail.text.data[0] == '<' || tail.text.data[0] == '>')
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("ordering comparisons are not supported"), text, tail.offset, 1);
        return Memmy_Status_ParseError;
    }

    Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                             String8_Lit("unexpected trailing input"), text, tail.offset, tail.text.len);
    return Memmy_Status_ParseError;
}

/*
memory_expr       = pattern_scan_expr | typed_expr | address_expr
pattern_scan_expr = range_expr, ws_opt, "{", ws_opt, pattern, ws_opt, "}"

Whitespace is allowed around top-level ":", "=", "==", pattern braces, inside
patterns, and inside const_expr.
*/
Memmy_Status Memmy_MemoryExpr_Parse(Arena *arena, String8 text, Memmy_MemoryExpr *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("missing arena or output memory expression"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_ExprSlice source = Memmy_Expr_TrimSlice(text, 0, text.len);
    if (source.text.len == 0)
    {
        Memmy_ExprError_SetInput(error, Memmy_Status_ParseError, String8_Lit("expr"),
                                 String8_Lit("expected memory expression"), text, source.offset, 0);
        return Memmy_Status_ParseError;
    }

    U64 pattern = 0;
    if (Memmy_Expr_TopLevelFind(source.text, String8_Lit("{"), &pattern))
    {
        Memmy_Status status = Memmy_MemoryExpr_ParsePattern(arena, text, source, out, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    U64 colon = 0;
    if (Memmy_Expr_TopLevelColon(source.text, &colon))
    {
        Memmy_Status status = Memmy_MemoryExpr_ParseTyped(arena, text, source, out, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (error != 0)
        {
            *error = (Memmy_Error){0};
        }
        return Memmy_Status_Ok;
    }

    Memmy_AddressExpr address = {0};
    Memmy_Status status = Memmy_AddressExpr_Parse(arena, source.text, &address, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Expr_RemapError(error, text, source.offset);
        return status;
    }

    *out = (Memmy_MemoryExpr){
        .kind = Memmy_MemoryExprKind_Address,
        .address = address,
    };
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}
