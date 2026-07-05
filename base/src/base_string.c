#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// String8
// ---------------------------------------------------------------------------

String8 String8_Make(U8 *data, U64 len)
{
    return (String8){.data = data, .len = len};
}

String8 String8_FromCStr(char *c)
{
    U64 len = 0;
    if (c != 0)
    {
        while (c[len] != 0)
        {
            len++;
        }
    }
    return (String8){.data = (U8 *)c, .len = len};
}

String8 String8_Copy(Arena *a, String8 s)
{
    U8 *data = Arena_PushArrayNoZero(a, U8, s.len + 1);
    memcpy(data, s.data, s.len);
    data[s.len] = 0;
    return (String8){.data = data, .len = s.len};
}

char *String8_ToCStr(Arena *a, String8 s)
{
    char *buf = Arena_PushArrayNoZero(a, char, s.len + 1);
    memcpy(buf, s.data, s.len);
    buf[s.len] = 0;
    return buf;
}

String8 String8_PushFV(Arena *a, char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    I32 needed = vsnprintf(0, 0, fmt, args2);
    va_end(args2);

    if (needed < 0)
    {
        return (String8){0};
    }

    U8 *data = Arena_PushArrayNoZero(a, U8, (U64)needed + 1);
    vsnprintf((char *)data, (U64)needed + 1, fmt, args);
    return (String8){.data = data, .len = (U64)needed};
}

String8 String8_PushF(Arena *a, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 result = String8_PushFV(a, fmt, args);
    va_end(args);
    return result;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

B32 String8_Eq(String8 a, String8 b)
{
    if (a.len != b.len)
    {
        return 0;
    }
    if (a.len == 0)
    {
        return 1;
    }
    return memcmp(a.data, b.data, a.len) == 0;
}

static U8 ToLower(U8 c)
{
    return (c >= 'A' && c <= 'Z') ? (U8)(c + 32) : c;
}

B32 String8_EqNoCase(String8 a, String8 b)
{
    if (a.len != b.len)
    {
        return 0;
    }
    for (U64 i = 0; i < a.len; i++)
    {
        if (ToLower(a.data[i]) != ToLower(b.data[i]))
        {
            return 0;
        }
    }
    return 1;
}

B32 String8_FuzzyMatchNoCase(String8 haystack, String8 needle)
{
    U64 needle_i = 0;
    for (U64 haystack_i = 0; haystack_i < haystack.len && needle_i < needle.len; haystack_i++)
    {
        if (ToLower(haystack.data[haystack_i]) == ToLower(needle.data[needle_i]))
        {
            needle_i++;
        }
    }
    return needle_i == needle.len;
}

// ---------------------------------------------------------------------------
// Slicing
// ---------------------------------------------------------------------------

String8 String8_Prefix(String8 s, U64 len)
{
    U64 clamped = Min(len, s.len);
    return (String8){.data = s.data, .len = clamped};
}

String8 String8_Suffix(String8 s, U64 len)
{
    U64 clamped = Min(len, s.len);
    return (String8){.data = s.data + s.len - clamped, .len = clamped};
}

String8 String8_Substr(String8 s, U64 offset, U64 len)
{
    if (offset >= s.len)
    {
        return (String8){0};
    }
    U64 max_len = s.len - offset;
    U64 clamped = Min(len, max_len);
    return (String8){.data = s.data + offset, .len = clamped};
}

static B32 IsWhitespace(U8 c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

String8 String8_TrimWhitespace(String8 s)
{
    if (s.len == 0)
    {
        return s;
    }
    U64 start = 0;
    U64 end = s.len;
    while (start < end && IsWhitespace(s.data[start]))
    {
        start++;
    }
    while (end > start && IsWhitespace(s.data[end - 1]))
    {
        end--;
    }
    return (String8){.data = s.data + start, .len = end - start};
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

B32 String8_StartsWith(String8 s, String8 prefix)
{
    if (prefix.len > s.len)
    {
        return 0;
    }
    return memcmp(s.data, prefix.data, prefix.len) == 0;
}

B32 String8_EndsWith(String8 s, String8 suffix)
{
    if (suffix.len > s.len)
    {
        return 0;
    }
    return memcmp(s.data + s.len - suffix.len, suffix.data, suffix.len) == 0;
}

U64 String8_Find(String8 haystack, String8 needle, U64 start)
{
    if (needle.len == 0)
    {
        return start <= haystack.len ? start : STRING8_NPOS;
    }
    if (needle.len > haystack.len || start > haystack.len - needle.len)
    {
        return STRING8_NPOS;
    }
    U64 last = haystack.len - needle.len;
    for (U64 i = start; i <= last; i++)
    {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
        {
            return i;
        }
    }
    return STRING8_NPOS;
}

U64 String8_FindChar(String8 s, U8 c, U64 start)
{
    for (U64 i = start; i < s.len; i++)
    {
        if (s.data[i] == c)
        {
            return i;
        }
    }
    return STRING8_NPOS;
}

U64 String8_FindLastChar(String8 s, U8 c)
{
    for (U64 i = s.len; i > 0; i--)
    {
        if (s.data[i - 1] == c)
        {
            return i - 1;
        }
    }
    return STRING8_NPOS;
}

// ---------------------------------------------------------------------------
// Splitting
// ---------------------------------------------------------------------------

static String8Slice Split_Collect(Arena *a, String8List *list)
{
    String8Slice slice;
    slice.count = list->list.count;
    slice.v = Arena_PushArrayNoZero(a, String8, slice.count);
    U64 i = 0;
    List_ForEach(String8Node, n, &list->list, link)
    {
        slice.v[i++] = n->str;
    }
    return slice;
}

String8Slice String8_Split(Arena *a, String8 s, String8 sep)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    String8List list = {0};
    if (sep.len == 0)
    {
        String8List_Push(scratch.arena, &list, s);
    }
    else
    {
        U64 pos = 0;
        while (pos <= s.len)
        {
            U64 hit = String8_Find(s, sep, pos);
            if (hit == STRING8_NPOS)
            {
                String8List_Push(scratch.arena, &list, String8_Substr(s, pos, s.len - pos));
                break;
            }
            String8List_Push(scratch.arena, &list, String8_Substr(s, pos, hit - pos));
            pos = hit + sep.len;
            if (pos == s.len)
            {
                String8List_Push(scratch.arena, &list, (String8){.data = s.data + s.len, .len = 0});
                break;
            }
        }
    }
    String8Slice out = Split_Collect(a, &list);
    Scratch_End(scratch);
    return out;
}

String8Slice String8_SplitChar(Arena *a, String8 s, U8 c)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    String8List list = {0};
    U64 start = 0;
    for (U64 i = 0; i <= s.len; i++)
    {
        if (i == s.len || s.data[i] == c)
        {
            String8List_Push(scratch.arena, &list, String8_Substr(s, start, i - start));
            start = i + 1;
        }
    }
    String8Slice out = Split_Collect(a, &list);
    Scratch_End(scratch);
    return out;
}

String8Slice String8_SplitLines(Arena *a, String8 s)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    String8List list = {0};
    U64 start = 0;
    for (U64 i = 0; i < s.len; i++)
    {
        if (s.data[i] == '\n')
        {
            U64 end = i;
            if (end > start && s.data[end - 1] == '\r')
            {
                end--;
            }
            String8List_Push(scratch.arena, &list, String8_Substr(s, start, end - start));
            start = i + 1;
        }
    }
    if (start <= s.len)
    {
        if (start < s.len || list.list.count == 0)
        {
            String8List_Push(scratch.arena, &list, String8_Substr(s, start, s.len - start));
        }
    }
    String8Slice out = Split_Collect(a, &list);
    Scratch_End(scratch);
    return out;
}

// ---------------------------------------------------------------------------
// Rewriting
// ---------------------------------------------------------------------------

String8 String8_Replace(Arena *a, String8 s, String8 from, String8 to)
{
    if (from.len == 0)
    {
        return String8_Copy(a, s);
    }
    Scratch scratch = Scratch_Begin(&a, 1);
    String8List parts = {0};
    U64 pos = 0;
    while (pos <= s.len)
    {
        U64 hit = String8_Find(s, from, pos);
        if (hit == STRING8_NPOS)
        {
            String8List_Push(scratch.arena, &parts, String8_Substr(s, pos, s.len - pos));
            break;
        }
        String8List_Push(scratch.arena, &parts, String8_Substr(s, pos, hit - pos));
        String8List_Push(scratch.arena, &parts, to);
        pos = hit + from.len;
    }
    String8 out = String8List_Join(a, &parts, (String8){0});
    Scratch_End(scratch);
    return out;
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

void String8List_Push(Arena *a, String8List *list, String8 s)
{
    String8Node *node = Arena_PushStruct(a, String8Node);
    node->str = s;
    List_PushBack(&list->list, &node->link);
    list->total_len += s.len;
}

String8 String8List_Join(Arena *a, String8List *list, String8 sep)
{
    U64 sep_total = (list->list.count > 1) ? sep.len * (list->list.count - 1) : 0;
    U64 total = list->total_len + sep_total;

    U8 *data = Arena_PushArrayNoZero(a, U8, total + 1);
    U64 pos = 0;

    List_ForEach(String8Node, n, &list->list, link)
    {
        memcpy(data + pos, n->str.data, n->str.len);
        pos += n->str.len;
        if (n->link.next != 0 && sep.len > 0)
        {
            memcpy(data + pos, sep.data, sep.len);
            pos += sep.len;
        }
    }
    data[total] = 0;
    return (String8){.data = data, .len = total};
}

// ---------------------------------------------------------------------------
// Conversion
// ---------------------------------------------------------------------------

U64 String8_ToU64(String8 s, U32 base)
{
    U64 result = 0;
    for (U64 i = 0; i < s.len; i++)
    {
        U8 c = s.data[i];
        U64 digit;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = 10 + (c - 'a');
        }
        else if (c >= 'A' && c <= 'F')
        {
            digit = 10 + (c - 'A');
        }
        else
        {
            break;
        }
        if (digit >= base)
        {
            break;
        }
        result = result * base + digit;
    }
    return result;
}

I64 String8_ToI64(String8 s, U32 base)
{
    if (s.len == 0)
    {
        return 0;
    }
    B32 neg = 0;
    U64 start = 0;
    if (s.data[0] == '-')
    {
        neg = 1;
        start = 1;
    }
    else if (s.data[0] == '+')
    {
        start = 1;
    }
    String8 digits = String8_Substr(s, start, s.len - start);
    U64 abs_val = String8_ToU64(digits, base);
    return neg ? -(I64)abs_val : (I64)abs_val;
}

String8_ParseStatus String8_ParseF64(String8 s, F64 *out, U64 *out_error_offset)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cstr = String8_ToCStr(scratch.arena, s);
    char *end = 0;
    errno = 0;
    F64 value = strtod(cstr, &end);
    U64 parsed_len = (end > cstr) ? (U64)(end - cstr) : 0;

    String8_ParseStatus status = String8_ParseStatus_Ok;
    U64 error_offset = 0;
    if (parsed_len == 0 || parsed_len != s.len)
    {
        status = String8_ParseStatus_Invalid;
        error_offset = parsed_len;
    }
    else if (errno == ERANGE)
    {
        status = String8_ParseStatus_Overflow;
        error_offset = parsed_len;
    }
    else if (out != 0)
    {
        *out = value;
    }

    if (out_error_offset != 0)
    {
        *out_error_offset = error_offset;
    }
    Scratch_End(scratch);
    return status;
}
