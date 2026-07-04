#include "test_memmy_common.h"

Test(Test_MemmyTypeParseAcceptsV0Spellings)
{
    struct
    {
        String8 text;
        Memmy_TypeKind kind;
        U64 fixed_size;
    } cases[] = {
        {String8_Lit("u8"), Memmy_TypeKind_U8, 1},   {String8_Lit("i8"), Memmy_TypeKind_I8, 1},
        {String8_Lit("u16"), Memmy_TypeKind_U16, 2}, {String8_Lit("i16"), Memmy_TypeKind_I16, 2},
        {String8_Lit("u32"), Memmy_TypeKind_U32, 4}, {String8_Lit("i32"), Memmy_TypeKind_I32, 4},
        {String8_Lit("u64"), Memmy_TypeKind_U64, 8}, {String8_Lit("i64"), Memmy_TypeKind_I64, 8},
        {String8_Lit("f32"), Memmy_TypeKind_F32, 4}, {String8_Lit("f64"), Memmy_TypeKind_F64, 8},
        {String8_Lit("ptr"), Memmy_TypeKind_Ptr, 0}, {String8_Lit("bytes"), Memmy_TypeKind_Bytes, 0},
        {String8_Lit("str"), Memmy_TypeKind_Str, 0}, {String8_Lit("wstr"), Memmy_TypeKind_WStr, 0},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Type type = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Type_Parse(cases[i].text, &type, &error), Memmy_Status_Ok);
        AssertEq(type.kind, cases[i].kind);
        AssertEq(type.fixed_size, cases[i].fixed_size);
    }
}

Test(Test_MemmyTypeParseRejectsUnknownNames)
{
    String8 cases[] = {String8_Lit("U8"), String8_Lit("int"), String8_Lit(""), String8_Lit("ptr32")};
    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Type type = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Type_Parse(cases[i], &type, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("type"));
        AssertStrEq(error.input, cases[i]);
    }
}

Test(Test_MemmyValueParseIntegerBounds)
{
    Arena *arena = Arena_CreateDefault();
    struct
    {
        String8 type_name;
        String8 text;
        U8 expected[8];
        U64 expected_len;
    } cases[] = {
        {String8_Lit("u8"), String8_Lit("255"), {0xff}, 1},
        {String8_Lit("i8"), String8_Lit("-128"), {0x80}, 1},
        {String8_Lit("i8"), String8_Lit("127"), {0x7f}, 1},
        {String8_Lit("u16"), String8_Lit("65535"), {0xff, 0xff}, 2},
        {String8_Lit("i16"), String8_Lit("-32768"), {0x00, 0x80}, 2},
        {String8_Lit("i16"), String8_Lit("32767"), {0xff, 0x7f}, 2},
        {String8_Lit("u32"), String8_Lit("4294967295"), {0xff, 0xff, 0xff, 0xff}, 4},
        {String8_Lit("i32"), String8_Lit("-2147483648"), {0x00, 0x00, 0x00, 0x80}, 4},
        {String8_Lit("i32"), String8_Lit("2147483647"), {0xff, 0xff, 0xff, 0x7f}, 4},
        {String8_Lit("u64"), String8_Lit("18446744073709551615"), {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 8},
        {String8_Lit("i64"), String8_Lit("-9223372036854775808"), {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}, 8},
        {String8_Lit("i64"), String8_Lit("9223372036854775807"), {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, 8},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Value value = {0};
        Memmy_Type type = {0};
        Test_ParseType(cases[i].type_name, &type);
        AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, cases[i].text, &value, 0), Memmy_Status_Ok);
        Test_AssertBytes(value.bytes, cases[i].expected, cases[i].expected_len);
    }
    Arena_Destroy(arena);
}

Test(Test_MemmyValueParseIntegerRejectsInvalidAndOutOfRange)
{
    Arena *arena = Arena_CreateDefault();
    struct
    {
        String8 type_name;
        String8 text;
    } cases[] = {
        {String8_Lit("u8"), String8_Lit("256")},
        {String8_Lit("i8"), String8_Lit("-129")},
        {String8_Lit("i8"), String8_Lit("128")},
        {String8_Lit("u16"), String8_Lit("65536")},
        {String8_Lit("i16"), String8_Lit("-32769")},
        {String8_Lit("i16"), String8_Lit("32768")},
        {String8_Lit("u32"), String8_Lit("4294967296")},
        {String8_Lit("i32"), String8_Lit("-2147483649")},
        {String8_Lit("i32"), String8_Lit("2147483648")},
        {String8_Lit("u64"), String8_Lit("18446744073709551616")},
        {String8_Lit("i64"), String8_Lit("-9223372036854775809")},
        {String8_Lit("i64"), String8_Lit("9223372036854775808")},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Value value = {0};
        Memmy_Error error = {0};
        Memmy_Type type = {0};
        Test_ParseType(cases[i].type_name, &type);
        AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, cases[i].text, &value, &error),
                 Memmy_Status_Overflow);
        AssertStrEq(error.context, String8_Lit("value"));
        AssertStrEq(error.input, cases[i].text);
    }
    Arena_Destroy(arena);
}

Test(Test_MemmyValueParseIntegerRejectsInvalidSyntaxAndWrongSigns)
{
    Arena *arena = Arena_CreateDefault();
    struct
    {
        String8 type_name;
        String8 text;
    } cases[] = {
        {String8_Lit("u32"), String8_Lit("+1")},  {String8_Lit("u32"), String8_Lit("-1")},
        {String8_Lit("u8"), String8_Lit("-1")},   {String8_Lit("u32"), String8_Lit("0x")},
        {String8_Lit("i32"), String8_Lit("12x")}, {String8_Lit("i32"), String8_Lit("-")},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Value value = {0};
        Memmy_Error error = {0};
        Memmy_Type type = {0};
        Test_ParseType(cases[i].type_name, &type);
        AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, cases[i].text, &value, &error),
                 Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("value"));
        AssertStrEq(error.input, cases[i].text);
    }
    Arena_Destroy(arena);
}

Test(Test_MemmyValueParsePointerWidths)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Type type = {0};
    Test_ParseType(String8_Lit("ptr"), &type);
    Memmy_Value value = {0};
    U8 expected32[] = {0x78, 0x56, 0x34, 0x12};
    U8 expected64[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_32, String8_Lit("0x12345678"), &value, 0),
             Memmy_Status_Ok);
    Test_AssertBytes(value.bytes, expected32, ArrayCount(expected32));

    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("0x0102030405060708"), &value, 0),
             Memmy_Status_Ok);
    Test_AssertBytes(value.bytes, expected64, ArrayCount(expected64));

    Memmy_Error error = {0};
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_32, String8_Lit("0x100000000"), &value, &error),
             Memmy_Status_Overflow);
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_Unknown, String8_Lit("0"), &value, &error),
             Memmy_Status_InvalidArgument);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueParseFloatsAndText)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Value value = {0};

    Memmy_Type type = {0};
    Test_ParseType(String8_Lit("f32"), &type);
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("1.5"), &value, 0), Memmy_Status_Ok);
    F32 f32 = 1.5f;
    Test_AssertBytes(value.bytes, (U8 *)&f32, sizeof(f32));

    Test_ParseType(String8_Lit("f64"), &type);
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("-2.25"), &value, 0), Memmy_Status_Ok);
    F64 f64 = -2.25;
    Test_AssertBytes(value.bytes, (U8 *)&f64, sizeof(f64));

    Test_ParseType(String8_Lit("str"), &type);
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("hello"), &value, 0), Memmy_Status_Ok);
    U8 str_expected[] = {'h', 'e', 'l', 'l', 'o'};
    Test_AssertBytes(value.bytes, str_expected, ArrayCount(str_expected));

    Test_ParseType(String8_Lit("wstr"), &type);
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("Az"), &value, 0), Memmy_Status_Ok);
    U8 wstr_expected[] = {'A', 0, 'z', 0};
    Test_AssertBytes(value.bytes, wstr_expected, ArrayCount(wstr_expected));
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
    AssertEq(pattern.bytes[1].value, 0x8b);
    AssertEq(pattern.bytes[2].value, 0xff);

    AssertEq(
        Memmy_Pattern_Parse(arena, String8_Lit("48 ?? 89"), Memmy_PatternParseFlag_AllowWildcards, &pattern, &error),
        Memmy_Status_Ok);
    AssertEq(pattern.count, 3);
    AssertTrue(pattern.bytes[1].wildcard);

    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("48 ? ?? ? 89"), Memmy_PatternParseFlag_AllowWildcards, &pattern,
                                 &error),
             Memmy_Status_Ok);
    AssertEq(pattern.count, 5);
    AssertEq(pattern.bytes[0].value, 0x48);
    AssertTrue(pattern.bytes[1].wildcard);
    AssertTrue(pattern.bytes[2].wildcard);
    AssertTrue(pattern.bytes[3].wildcard);
    AssertEq(pattern.bytes[4].value, 0x89);

    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("48 ?? 89"), 0, &pattern, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("pattern"));
    AssertEq(error.byte_offset, 3);

    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("48 ? 89"), 0, &pattern, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("pattern"));
    AssertEq(error.byte_offset, 3);

    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("4 8b"), 0, &pattern, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("gg"), 0, &pattern, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Pattern_Parse(arena, String8_Lit("?F"), Memmy_PatternParseFlag_AllowWildcards, &pattern, &error),
             Memmy_Status_ParseError);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueParseBytesRejectsWildcards)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Type type = {0};
    Test_ParseType(String8_Lit("bytes"), &type);
    Memmy_Value value = {0};
    U8 expected[] = {0x90, 0x90, 0xcc};

    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("90 90 cc"), &value, 0),
             Memmy_Status_Ok);
    Test_AssertBytes(value.bytes, expected, ArrayCount(expected));

    Memmy_Error error = {0};
    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("90 ?? cc"), &value, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("pattern"));

    AssertEq(Memmy_Value_Parse(arena, type, Memmy_PointerWidth_64, String8_Lit("90 ? cc"), &value, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("pattern"));
}
TestSuite suite_memmy_value = TestSuite_Make(
    "Memmy Value", TestCase_Make(Test_MemmyTypeParseAcceptsV0Spellings),
    TestCase_Make(Test_MemmyTypeParseRejectsUnknownNames), TestCase_Make(Test_MemmyValueParseIntegerBounds),
    TestCase_Make(Test_MemmyValueParseIntegerRejectsInvalidAndOutOfRange),
    TestCase_Make(Test_MemmyValueParseIntegerRejectsInvalidSyntaxAndWrongSigns),
    TestCase_Make(Test_MemmyValueParsePointerWidths), TestCase_Make(Test_MemmyValueParseFloatsAndText),
    TestCase_Make(Test_MemmyPatternParseBytesWildcardsAndRejection),
    TestCase_Make(Test_MemmyValueParseBytesRejectsWildcards), );
