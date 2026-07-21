#ifndef MEMMY_VALUE_H
#define MEMMY_VALUE_H

#include "base.h"
#include "memmy_process.h"
#include "memmy_status.h"

typedef U32 Memmy_TypeKind;
enum
{
    Memmy_TypeKind_Null,
    Memmy_TypeKind_Integer,
    Memmy_TypeKind_Float,
    Memmy_TypeKind_Address,
    Memmy_TypeKind_String,
    Memmy_TypeKind_Range,
    Memmy_TypeKind_List,
};

typedef U32 Memmy_StringEncoding;
enum
{
    Memmy_StringEncoding_Utf8,
    Memmy_StringEncoding_Utf16Le,
};

typedef struct Memmy_Type Memmy_Type;
struct Memmy_Type
{
    Memmy_TypeKind kind;
    union {
        struct
        {
            U32 bit_count;
            B32 is_signed;
        } integer;
        struct
        {
            U32 bit_count;
        } floating;
        struct
        {
            Memmy_StringEncoding encoding;
            B32 zero_terminated;
        } string;
        struct
        {
            Memmy_Type const *element_type;
        } list;
    };
};

extern Memmy_Type const Memmy_Type_Null;
extern Memmy_Type const Memmy_Type_U8;
extern Memmy_Type const Memmy_Type_I8;
extern Memmy_Type const Memmy_Type_U16;
extern Memmy_Type const Memmy_Type_I16;
extern Memmy_Type const Memmy_Type_U32;
extern Memmy_Type const Memmy_Type_I32;
extern Memmy_Type const Memmy_Type_U64;
extern Memmy_Type const Memmy_Type_I64;
extern Memmy_Type const Memmy_Type_F32;
extern Memmy_Type const Memmy_Type_F64;
extern Memmy_Type const Memmy_Type_Address;
extern Memmy_Type const Memmy_Type_Str;
extern Memmy_Type const Memmy_Type_WStr;
extern Memmy_Type const Memmy_Type_Range;

typedef struct Memmy_ValueList Memmy_ValueList;
struct Memmy_ValueList
{
    U64 count;
    union {
        I64 *signed_integers;
        U64 *unsigned_integers;
        U32 *floating32_bits;
        U64 *floating64_bits;
        Memmy_Addr *addresses;
        Memmy_Range *ranges;
        String8 *strings;
    };
};

typedef struct Memmy_Value Memmy_Value;
struct Memmy_Value
{
    Memmy_Type type;
    union {
        I64 signed_integer;
        U64 unsigned_integer;
        U64 floating_bits;
        Memmy_Addr address;
        Memmy_Range range;
        String8 string;
        Memmy_ValueList list;
    };
};

typedef struct Memmy_EncodedValue Memmy_EncodedValue;
struct Memmy_EncodedValue
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

B32 Memmy_Type_IsValid(Memmy_Type type);
B32 Memmy_Type_IsNull(Memmy_Type type);
B32 Memmy_Type_IsInteger(Memmy_Type type);
B32 Memmy_Type_IsFloat(Memmy_Type type);
B32 Memmy_Type_IsAddress(Memmy_Type type);
B32 Memmy_Type_IsString(Memmy_Type type);
B32 Memmy_Type_IsRange(Memmy_Type type);
B32 Memmy_Type_IsList(Memmy_Type type);
B32 Memmy_Type_Eq(Memmy_Type a, Memmy_Type b);
U64 Memmy_Type_EncodedSize(Memmy_Type type);
Memmy_Status Memmy_Type_ListCreate(Arena *arena, Memmy_Type element_type, Memmy_Type *out, Memmy_Error *error);
Memmy_Status Memmy_Type_Parse(String8 text, Memmy_Type *out, Memmy_Error *error);

Memmy_Status Memmy_Value_Copy(Arena *arena, Memmy_Value const *value, Memmy_Value *out, Memmy_Error *error);
Memmy_Status Memmy_Value_Decode(Arena *arena, Memmy_Type type, String8 encoded, Memmy_Value *out, Memmy_Error *error);
Memmy_Status Memmy_Value_Convert(Arena *arena, Memmy_Value const *value, Memmy_Type destination_type, Memmy_Value *out,
                                 Memmy_Error *error);
Memmy_Status Memmy_Value_Encode(Arena *arena, Memmy_Value const *value, Memmy_EncodedValue *out, Memmy_Error *error);

// Transitional text parser for command/evaluator callers. Encoded storage belongs to arena.
Memmy_Status Memmy_EncodedValue_Parse(Arena *arena, Memmy_Type type, String8 text, Memmy_EncodedValue *out,
                                      Memmy_Error *error);
// Pattern byte storage belongs to arena. Text is borrowed only for the duration of the call.
Memmy_Status Memmy_Pattern_Parse(Arena *arena, String8 text, Memmy_PatternParseFlags flags, Memmy_Pattern *out,
                                 Memmy_Error *error);

#endif // MEMMY_VALUE_H
