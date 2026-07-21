#include "test_memmy_common.h"

#include <math.h>

Test(Test_MemmyTypeDescriptorsParseValidateAndCompareStructurally)
{
    struct
    {
        String8 text;
        Memmy_Type const *expected;
    } cases[] = {
        {String8_Lit("u8"), &Memmy_Type_U8},   {String8_Lit("i8"), &Memmy_Type_I8},
        {String8_Lit("u16"), &Memmy_Type_U16}, {String8_Lit("i16"), &Memmy_Type_I16},
        {String8_Lit("u32"), &Memmy_Type_U32}, {String8_Lit("i32"), &Memmy_Type_I32},
        {String8_Lit("u64"), &Memmy_Type_U64}, {String8_Lit("i64"), &Memmy_Type_I64},
        {String8_Lit("f32"), &Memmy_Type_F32}, {String8_Lit("f64"), &Memmy_Type_F64},
        {String8_Lit("str"), &Memmy_Type_Str}, {String8_Lit("wstr"), &Memmy_Type_WStr},
    };
    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Type type = {0};
        AssertEq(Memmy_Type_Parse(cases[i].text, &type, 0), Memmy_Status_Ok);
        AssertTrue(Memmy_Type_Eq(type, *cases[i].expected));
        AssertTrue(Memmy_Type_IsValid(type));
    }
    AssertTrue(!Memmy_Type_Eq(Memmy_Type_U8, Memmy_Type_U64));
    AssertTrue(!Memmy_Type_Eq(Memmy_Type_I32, Memmy_Type_U32));
    AssertTrue(!Memmy_Type_Eq(Memmy_Type_F32, Memmy_Type_F64));
    AssertTrue(!Memmy_Type_Eq(Memmy_Type_Str, Memmy_Type_WStr));
    AssertTrue(!Memmy_Type_IsValid((Memmy_Type){.kind = Memmy_TypeKind_Integer, .integer = {.bit_count = 24}}));
    AssertTrue(!Memmy_Type_IsValid((Memmy_Type){.kind = Memmy_TypeKind_Float, .floating = {.bit_count = 16}}));

    String8 rejected[] = {String8_Lit("ptr"), String8_Lit("bytes"), String8_Lit("list<u8>"), String8_Lit("U8")};
    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_Type type = Memmy_Type_U8;
        Memmy_Error error = {0};
        AssertEq(Memmy_Type_Parse(rejected[i], &type, &error), Memmy_Status_ParseError);
        AssertTrue(Memmy_Type_IsNull(type));
        AssertStrEq(error.context, String8_Lit("type"));
    }
}

Test(Test_MemmyTypeListConstructionAndValueDeepCopy)
{
    Arena *source = Arena_CreateDefault();
    Arena *destination = Arena_CreateDefault();
    Memmy_Type list_type = {0};
    AssertEq(Memmy_Type_ListCreate(source, Memmy_Type_Str, &list_type, 0), Memmy_Status_Ok);
    Memmy_Type equivalent = {.kind = Memmy_TypeKind_List, .list = {.element_type = &Memmy_Type_Str}};
    AssertTrue(Memmy_Type_Eq(list_type, equivalent));
    AssertEq(Memmy_Type_ListCreate(source, Memmy_Type_Null, &equivalent, 0), Memmy_Status_InvalidArgument);
    AssertEq(Memmy_Type_ListCreate(source, list_type, &equivalent, 0), Memmy_Status_InvalidArgument);

    String8 *strings = Arena_PushArray(source, String8, 2);
    strings[0] = String8_Copy(source, String8_Lit("alpha"));
    strings[1] = String8_Copy(source, String8_Lit("beta"));
    Memmy_Value value = {.type = list_type, .list = {.count = 2, .strings = strings}};
    Memmy_Value copy = {0};
    AssertEq(Memmy_Value_Copy(destination, &value, &copy, 0), Memmy_Status_Ok);
    AssertTrue(Memmy_Type_Eq(copy.type, list_type));
    AssertTrue(copy.type.list.element_type != list_type.list.element_type);
    AssertTrue(copy.list.strings != strings);
    AssertTrue(copy.list.strings[0].data != strings[0].data);
    AssertStrEq(copy.list.strings[0], String8_Lit("alpha"));
    AssertStrEq(copy.list.strings[1], String8_Lit("beta"));
    Arena_Destroy(source);
    AssertStrEq(copy.list.strings[0], String8_Lit("alpha"));
    Arena_Destroy(destination);
}

Test(Test_MemmyValueIntegerDecodeEncodeBoundaries)
{
    Arena *arena = Arena_CreateDefault();
    struct
    {
        Memmy_Type const *type;
        U8 bytes[8];
        I64 signed_value;
        U64 unsigned_value;
    } cases[] = {
        {&Memmy_Type_U8, {0xff}, 0, U8_MAX},
        {&Memmy_Type_I8, {0x80}, I8_MIN, 0},
        {&Memmy_Type_U16, {0xff, 0xff}, 0, U16_MAX},
        {&Memmy_Type_I16, {0x00, 0x80}, I16_MIN, 0},
        {&Memmy_Type_U32, {0xff, 0xff, 0xff, 0xff}, 0, U32_MAX},
        {&Memmy_Type_I32, {0x00, 0x00, 0x00, 0x80}, I32_MIN, 0},
        {&Memmy_Type_U64, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 0, U64_MAX},
        {&Memmy_Type_I64, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, I64_MIN, 0},
    };
    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        U64 size = Memmy_Type_EncodedSize(*cases[i].type);
        Memmy_Value value = {0};
        AssertEq(Memmy_Value_Decode(arena, *cases[i].type, String8_Make(cases[i].bytes, size), &value, 0),
                 Memmy_Status_Ok);
        if (cases[i].type->integer.is_signed)
        {
            AssertEq(value.signed_integer, cases[i].signed_value);
        }
        else
        {
            AssertEq(value.unsigned_integer, cases[i].unsigned_value);
        }
        Memmy_EncodedValue encoded = {0};
        AssertEq(Memmy_Value_Encode(arena, &value, &encoded, 0), Memmy_Status_Ok);
        Test_AssertBytes(encoded.bytes, cases[i].bytes, size);
    }
    Memmy_Value value = {0};
    AssertEq(Memmy_Value_Decode(arena, Memmy_Type_U32, String8_Lit("\x01"), &value, 0), Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueFloatBitsAndUtfRoundTrip)
{
    Arena *arena = Arena_CreateDefault();
    U32 f32_cases[] = {0x80000000u, 0x7fc12345u};
    for (U64 i = 0; i < ArrayCount(f32_cases); i++)
    {
        Memmy_Value value = {0};
        AssertEq(Memmy_Value_Decode(arena, Memmy_Type_F32, String8_Make((U8 *)&f32_cases[i], sizeof(f32_cases[i])),
                                    &value, 0),
                 Memmy_Status_Ok);
        AssertEq(value.floating_bits, f32_cases[i]);
        Memmy_EncodedValue encoded = {0};
        AssertEq(Memmy_Value_Encode(arena, &value, &encoded, 0), Memmy_Status_Ok);
        Test_AssertBytes(encoded.bytes, (U8 *)&f32_cases[i], sizeof(f32_cases[i]));
    }
    U64 f64_bits = 0x7ff8000012345678ull;
    Memmy_Value f64 = {0};
    AssertEq(Memmy_Value_Decode(arena, Memmy_Type_F64, String8_Make((U8 *)&f64_bits, 8), &f64, 0), Memmy_Status_Ok);
    AssertEq(f64.floating_bits, f64_bits);

    String8 text = String8_Lit("A\xc3\xa9\xf0\x9f\x98\x80");
    Memmy_Value string = {.type = Memmy_Type_Str, .string = text};
    Memmy_EncodedValue utf8 = {0};
    AssertEq(Memmy_Value_Encode(arena, &string, &utf8, 0), Memmy_Status_Ok);
    AssertEq(utf8.bytes.len, text.len + 1);
    AssertEq(utf8.bytes.data[utf8.bytes.len - 1], 0);
    Memmy_Value decoded = {0};
    AssertEq(Memmy_Value_Decode(arena, Memmy_Type_Str, utf8.bytes, &decoded, 0), Memmy_Status_Ok);
    AssertStrEq(decoded.string, text);

    string.type = Memmy_Type_WStr;
    Memmy_EncodedValue utf16 = {0};
    AssertEq(Memmy_Value_Encode(arena, &string, &utf16, 0), Memmy_Status_Ok);
    AssertEq(utf16.bytes.data[utf16.bytes.len - 1], 0);
    AssertEq(utf16.bytes.data[utf16.bytes.len - 2], 0);
    AssertEq(Memmy_Value_Decode(arena, Memmy_Type_WStr, utf16.bytes, &decoded, 0), Memmy_Status_Ok);
    AssertStrEq(decoded.string, text);

    U8 malformed_utf8[] = {0xc0, 0x80, 0};
    AssertEq(
        Memmy_Value_Decode(arena, Memmy_Type_Str, String8_Make(malformed_utf8, sizeof(malformed_utf8)), &decoded, 0),
        Memmy_Status_InvalidEncoding);
    U8 malformed_utf16[] = {0x00, 0xd8, 0, 0};
    AssertEq(
        Memmy_Value_Decode(arena, Memmy_Type_WStr, String8_Make(malformed_utf16, sizeof(malformed_utf16)), &decoded, 0),
        Memmy_Status_InvalidEncoding);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueConversionsAndRejectedFamilies)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Value signed_value = {.type = Memmy_Type_I64, .signed_integer = -1};
    Memmy_Value converted = {0};
    AssertEq(Memmy_Value_Convert(arena, &signed_value, Memmy_Type_U8, &converted, 0), Memmy_Status_Ok);
    AssertEq(converted.unsigned_integer, U8_MAX);
    AssertEq(Memmy_Value_Convert(arena, &signed_value, Memmy_Type_I8, &converted, 0), Memmy_Status_Ok);
    AssertEq(converted.signed_integer, -1);

    Memmy_Value large = {.type = Memmy_Type_U64, .unsigned_integer = U64_MAX};
    AssertEq(Memmy_Value_Convert(arena, &large, Memmy_Type_I64, &converted, 0), Memmy_Status_Overflow);
    Memmy_Value i32 = {.type = Memmy_Type_I32, .signed_integer = 42};
    AssertEq(Memmy_Value_Convert(arena, &i32, Memmy_Type_F32, &converted, 0), Memmy_Status_Ok);
    AssertEq(Memmy_Value_Convert(arena, &converted, Memmy_Type_I16, &converted, 0), Memmy_Status_Ok);
    AssertEq(converted.signed_integer, 42);

    F64 nan = NAN;
    U64 nan_bits = 0;
    Memory_Copy(&nan_bits, &nan, sizeof(nan_bits));
    Memmy_Value float_value = {.type = Memmy_Type_F64, .floating_bits = nan_bits};
    AssertEq(Memmy_Value_Convert(arena, &float_value, Memmy_Type_I64, &converted, 0), Memmy_Status_Overflow);

    Memmy_Value string = {.type = Memmy_Type_Str, .string = String8_Lit("hello")};
    AssertEq(Memmy_Value_Convert(arena, &string, Memmy_Type_WStr, &converted, 0), Memmy_Status_Ok);
    AssertTrue(Memmy_Type_Eq(converted.type, Memmy_Type_WStr));
    AssertStrEq(converted.string, String8_Lit("hello"));
    AssertEq(Memmy_Value_Convert(arena, &string, Memmy_Type_U64, &converted, 0), Memmy_Status_InvalidArgument);

    Memmy_Value address = {.type = Memmy_Type_Address, .address = 1};
    Memmy_EncodedValue encoded = {0};
    AssertEq(Memmy_Value_Encode(arena, &address, &encoded, 0), Memmy_Status_InvalidArgument);
    Memmy_Value range = {.type = Memmy_Type_Range, .range = {.start = 1, .end = 2}};
    AssertEq(Memmy_Value_Encode(arena, &range, &encoded, 0), Memmy_Status_InvalidArgument);
    Memmy_Value null = {0};
    AssertEq(Memmy_Value_Encode(arena, &null, &encoded, 0), Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

Test(Test_MemmyPatternParseBytesWildcardsAndRejection)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Pattern pattern = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("48 8b FF"), 0, &pattern, &error), Memmy_Status_Ok);
    AssertEq(pattern.count, 3);
    AssertEq(pattern.bytes[0].value, 0x48);
    AssertEq(pattern.bytes[2].value, 0xff);
    AssertEq(
        Memmy_Pattern_Parse(arena, String8_Lit("48 ?? 89"), Memmy_PatternParseFlag_AllowWildcards, &pattern, &error),
        Memmy_Status_Ok);
    AssertTrue(pattern.bytes[1].wildcard);
    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("48 ?? 89"), 0, &pattern, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("gg"), 0, &pattern, &error), Memmy_Status_ParseError);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_value = TestSuite_Make(
    "Memmy Value", TestCase_Make(Test_MemmyTypeDescriptorsParseValidateAndCompareStructurally),
    TestCase_Make(Test_MemmyTypeListConstructionAndValueDeepCopy),
    TestCase_Make(Test_MemmyValueIntegerDecodeEncodeBoundaries), TestCase_Make(Test_MemmyValueFloatBitsAndUtfRoundTrip),
    TestCase_Make(Test_MemmyValueConversionsAndRejectedFamilies),
    TestCase_Make(Test_MemmyPatternParseBytesWildcardsAndRejection), );
