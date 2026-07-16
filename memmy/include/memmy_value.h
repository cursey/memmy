#ifndef MEMMY_VALUE_H
#define MEMMY_VALUE_H

#include "base.h"
#include "memmy_process.h"
#include "memmy_status.h"

typedef U32 Memmy_TypeKind;
enum
{
    Memmy_TypeKind_Null,
    Memmy_TypeKind_U8,
    Memmy_TypeKind_I8,
    Memmy_TypeKind_U16,
    Memmy_TypeKind_I16,
    Memmy_TypeKind_U32,
    Memmy_TypeKind_I32,
    Memmy_TypeKind_U64,
    Memmy_TypeKind_I64,
    Memmy_TypeKind_F32,
    Memmy_TypeKind_F64,
    Memmy_TypeKind_Ptr,
    Memmy_TypeKind_Bytes,
    Memmy_TypeKind_Str,
    Memmy_TypeKind_WStr,
};

typedef struct Memmy_Type Memmy_Type;
struct Memmy_Type
{
    Memmy_TypeKind kind;
    U64 fixed_size;
};

typedef struct Memmy_Value Memmy_Value;
struct Memmy_Value
{
    Memmy_Type type;
    String8 bytes;
};

typedef struct Memmy_PatternByte Memmy_PatternByte;
struct Memmy_PatternByte
{
    U8 value;
    B32 wildcard;
};

typedef struct Memmy_Pattern Memmy_Pattern;
struct Memmy_Pattern
{
    Memmy_PatternByte *bytes;
    U64 count;
};

typedef U32 Memmy_PatternParseFlags;
enum
{
    Memmy_PatternParseFlag_None = 0,
    Memmy_PatternParseFlag_AllowWildcards = 1u << 0,
};

Memmy_Status Memmy_Type_Parse(String8 text, Memmy_Type *out, Memmy_Error *error);
// Parsed byte storage belongs to arena. Text is borrowed only for the duration of the call.
// Required outputs are initialized before validation; error is optional.
Memmy_Status Memmy_Value_Parse(Arena *arena, Memmy_Type type, Memmy_PointerWidth pointer_width, String8 text,
                               Memmy_Value *out, Memmy_Error *error);
// Pattern byte storage belongs to arena. Text is borrowed only for the duration of the call.
Memmy_Status Memmy_Pattern_Parse(Arena *arena, String8 text, Memmy_PatternParseFlags flags, Memmy_Pattern *out,
                                 Memmy_Error *error);

#endif // MEMMY_VALUE_H
