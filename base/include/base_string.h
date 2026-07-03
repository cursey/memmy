#ifndef BASE_STRING_H
#define BASE_STRING_H

#include <stdarg.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_list.h"

// ---------------------------------------------------------------------------
// String8
// ---------------------------------------------------------------------------

typedef struct String8 String8;
struct String8
{
    U8 *data;
    U64 len;
};

typedef struct String8Node String8Node;
struct String8Node
{
    ListLink link;
    String8 str;
};

typedef struct String8List String8List;
struct String8List
{
    List list; // String8Node
    U64 total_len;
};

typedef struct String8Slice String8Slice;
struct String8Slice
{
    String8 *v;
    U64 count;
};

#define STRING8_NPOS ((U64) - 1)

#define String8_Lit(s) ((String8){.data = (U8 *)(s), .len = sizeof(s) - 1})

// Character classification
#define Char8_IsDigit(c) ((c) >= '0' && (c) <= '9')
#define Char8_IsAlpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define Char8_IsAlnum(c) (Char8_IsAlpha(c) || Char8_IsDigit(c))

// Construction
String8 String8_Make(U8 *data, U64 len);
String8 String8_FromCStr(char *c);
char *String8_ToCStr(Arena *a, String8 s);
String8 String8_Copy(Arena *a, String8 s);
String8 String8_PushF(Arena *a, char *fmt, ...);
String8 String8_PushFV(Arena *a, char *fmt, va_list args);

// Comparison
B32 String8_Eq(String8 a, String8 b);
B32 String8_EqNoCase(String8 a, String8 b);

// Slicing
String8 String8_Prefix(String8 s, U64 len);
String8 String8_Suffix(String8 s, U64 len);
String8 String8_Substr(String8 s, U64 offset, U64 len);
String8 String8_TrimWhitespace(String8 s);

// Search
B32 String8_StartsWith(String8 s, String8 prefix);
B32 String8_EndsWith(String8 s, String8 suffix);
U64 String8_Find(String8 haystack, String8 needle, U64 start);
U64 String8_FindChar(String8 s, U8 c, U64 start);
U64 String8_FindLastChar(String8 s, U8 c);

// Splitting
String8Slice String8_Split(Arena *a, String8 s, String8 sep);
String8Slice String8_SplitChar(Arena *a, String8 s, U8 c);
String8Slice String8_SplitLines(Arena *a, String8 s);

// Rewriting
String8 String8_Replace(Arena *a, String8 s, String8 from, String8 to);

// List
void String8List_Push(Arena *a, String8List *list, String8 s);
String8 String8List_Join(Arena *a, String8List *list, String8 sep);

// Conversion
U64 String8_ToU64(String8 s, U32 base);
I64 String8_ToI64(String8 s, U32 base);

#endif // BASE_STRING_H
