#include "base_os.h"
#include "memmy.h"
#include "memmy_cli.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

#include <string.h>

Test(Test_MemmyHeaderExportsBaseTypes)
{
    U64 value = 42;
    AssertEq(value, 42);

    Memmy_Addr addr = 0x1000;
    Memmy_Size size = 16;
    Memmy_ProcessList processes = {0};
    Memmy_ModuleList modules = {0};
    Memmy_RegionList regions = {0};
    AssertEq(addr, 0x1000);
    AssertEq(size, 16);
    AssertEq(processes.list.count, 0);
    AssertEq(modules.list.count, 0);
    AssertEq(regions.list.count, 0);
}

Test(Test_MemmyStatusAndErrorHelpers)
{
    AssertStrEq(Memmy_Status_String(Memmy_Status_ParseError), String8_Lit("parse_error"));
    AssertStrEq(Memmy_Status_String((Memmy_Status)9999), String8_Lit("unknown"));

    Memmy_Error error = {0};
    Memmy_Error_Set(&error, Memmy_Status_Unsupported, String8_Lit("backend"), String8_Lit("no callback"));
    AssertEq(error.status, Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));
    AssertStrEq(error.message, String8_Lit("no callback"));
}

static void Test_AssertBytes(String8 actual, U8 *expected, U64 expected_len)
{
    AssertEq(actual.len, expected_len);
    for (U64 i = 0; i < expected_len; i++)
    {
        AssertEq(actual.data[i], expected[i]);
    }
}

static void Test_ParseType(String8 text, Memmy_Type *out)
{
    AssertEq(Memmy_Type_Parse(text, out, 0), Memmy_Status_Ok);
}

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
    Arena_Destroy(arena);
}

Test(Test_MemmyParseAddressAcceptsUnsignedTokens)
{
    Memmy_Addr addr = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseAddress(String8_Lit("0x000001d856780004"), &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x000001d856780004ull);

    AssertEq(Memmy_ParseAddress(String8_Lit("0X1000"), &addr, &error), Memmy_Status_Ok);
    AssertEq(addr, 0x1000);

    AssertEq(Memmy_ParseAddress(String8_Lit("4096"), &addr, &error), Memmy_Status_Ok);
    AssertEq(addr, 4096);
}

Test(Test_MemmyParseAddressRejectsExpressionsAndNames)
{
    String8 rejected[] = {
        String8_Lit("-1"),         String8_Lit("+1"), String8_Lit("0x1000+4"), String8_Lit("(0x1000)"),
        String8_Lit("client.dll"), String8_Lit("0x"), String8_Lit(""),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_Addr addr = 123;
        Memmy_Error error = {0};
        AssertEq(Memmy_ParseAddress(rejected[i], &addr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("address"));
        AssertStrEq(error.input, rejected[i]);
        AssertEq(addr, 123);
    }
}

Test(Test_MemmyParseAddressRejectsOverflow)
{
    Memmy_Addr addr = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseAddress(String8_Lit("18446744073709551616"), &addr, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    AssertEq(Memmy_ParseAddress(String8_Lit("0x10000000000000000"), &addr, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));
}

Test(Test_MemmyParseSizeAcceptsDecimalAndHexAndRejectsOverflow)
{
    Memmy_Size size = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseSize(String8_Lit("4096"), &size, &error), Memmy_Status_Ok);
    AssertEq(size, 4096);

    AssertEq(Memmy_ParseSize(String8_Lit("0x1000"), &size, &error), Memmy_Status_Ok);
    AssertEq(size, 0x1000);

    AssertEq(Memmy_ParseSize(String8_Lit("18446744073709551616"), &size, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyRangeFromStartEndValidatesOrder)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Range_FromStartEnd(0x1000, 0x2000, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x2000);

    AssertEq(Memmy_Range_FromStartEnd(0x1000, 0x1000, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1000);

    AssertEq(Memmy_Range_FromStartEnd(0x2000, 0x1000, &range, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyRangeFromStartLengthAllowsZeroAndRejectsOverflow)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Range_FromStartLength(0x1000, 0, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1000);

    AssertEq(Memmy_Range_FromStartLength(0x1000, 0x20, &range, &error), Memmy_Status_Ok);
    AssertEq(range.end, 0x1020);

    AssertEq(Memmy_Range_FromStartLength(U64_MAX, 1, &range, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyCliParseRangeOptionsAcceptsValidShapes)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};
    char *start_end[] = {"--start", "0x1000", "--end", "0x1020"};
    char *start_length[] = {"--start", "0x1000", "--length", "32"};

    AssertEq(Memmy_Cli_ParseRangeOptions((I32)ArrayCount(start_end), start_end, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1020);

    AssertEq(Memmy_Cli_ParseRangeOptions((I32)ArrayCount(start_length), start_length, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1020);
}

Test(Test_MemmyCliParseRangeOptionsRejectsInvalidCombinations)
{
    char *missing_start[] = {"--end", "0x1020"};
    char *both_end_length[] = {"--start", "0x1000", "--end", "0x1020", "--length", "0x20"};
    char *neither_end_length[] = {"--start", "0x1000"};
    char *missing_start_value[] = {"--start", "--end", "0x1020"};
    char *missing_end_value[] = {"--start", "0x1000", "--end"};
    char *duplicate_start[] = {"--start", "0x1000", "--start", "0x1000", "--end", "0x1020"};
    char *end_before_start[] = {"--start", "0x2000", "--end", "0x1000"};
    char *bad_length[] = {"--start", "0x1000", "--length", "-1"};

    struct
    {
        char **argv;
        I32 argc;
        Memmy_Status status;
    } cases[] = {
        {missing_start, (I32)ArrayCount(missing_start), Memmy_Status_ParseError},
        {both_end_length, (I32)ArrayCount(both_end_length), Memmy_Status_ParseError},
        {neither_end_length, (I32)ArrayCount(neither_end_length), Memmy_Status_ParseError},
        {missing_start_value, (I32)ArrayCount(missing_start_value), Memmy_Status_ParseError},
        {missing_end_value, (I32)ArrayCount(missing_end_value), Memmy_Status_ParseError},
        {duplicate_start, (I32)ArrayCount(duplicate_start), Memmy_Status_ParseError},
        {end_before_start, (I32)ArrayCount(end_before_start), Memmy_Status_InvalidArgument},
        {bad_length, (I32)ArrayCount(bad_length), Memmy_Status_ParseError},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Range range = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Cli_ParseRangeOptions(cases[i].argc, cases[i].argv, &range, &error), cases[i].status);
        AssertTrue(error.status != Memmy_Status_Ok);
    }
}

static void Test_DisableListRegions(Test_MemmyBackend *backend)
{
    backend->backend.capabilities &= ~Memmy_BackendCap_ListRegions;
    backend->backend.list_regions = 0;
}

static void Test_ResetOpenTracking(Test_MemmyBackend *backend)
{
    backend->open_call_count = 0;
    backend->last_open_pid = 0;
    backend->last_open_access = 0;
}

static void Test_ParsePattern(Arena *arena, char *text, Memmy_Pattern *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Pattern_Parse(arena, String8_FromCStr(text), Memmy_PatternParseFlag_AllowWildcards, out, &error),
             Memmy_Status_Ok);
}

static void Test_ParseValue(Arena *arena, char *type_text, Memmy_PointerWidth pointer_width, char *value_text,
                            Memmy_Value *out)
{
    Memmy_Error error = {0};
    Memmy_Type type = {0};
    AssertEq(Memmy_Type_Parse(String8_FromCStr(type_text), &type, &error), Memmy_Status_Ok);
    AssertEq(Memmy_Value_Parse(arena, type, pointer_width, String8_FromCStr(value_text), out, &error), Memmy_Status_Ok);
}

static void Test_OpenProcess(Arena *arena, Memmy_Process **out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read, out, &error), Memmy_Status_Ok);
}

static void Test_AssertScanAddresses(Memmy_ScanResultList *results, Memmy_Addr *addresses, U64 count)
{
    AssertEq(results->list.count, count);
    U64 index = 0;
    List_ForEach(Memmy_ScanResult, result, &results->list, link)
    {
        AssertTrue(index < count);
        AssertEq(result->address, addresses[index]);
        index++;
    }
}

Test(Test_MemmyScanFindsBeginningMiddleAndEnd)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 pattern_bytes[] = {0xaa, 0xbb, 0xcc};
    memcpy(test_backend.memory + 0, pattern_bytes, sizeof(pattern_bytes));
    memcpy(test_backend.memory + 0x40, pattern_bytes, sizeof(pattern_bytes));
    memcpy(test_backend.memory + 0xfd, pattern_bytes, sizeof(pattern_bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "aa bb cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1100}, .chunk_size = 0x20};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1000, 0x1040, 0x10fd};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanDoesNotReadOutsideRequestedRangeAndAllowsZeroLength)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "10 11", &pattern);
    Memmy_Error error = {0};
    Memmy_ScanResultList results = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 7};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    AssertTrue(test_backend.min_read_addr >= options.range.start);
    AssertTrue(test_backend.max_read_end <= options.range.end);

    test_backend.read_call_count = 0;
    Test_ParsePattern(arena, "10 11 12", &pattern);
    options = (Memmy_ScanOptions){.range = {.start = 0x1010, .end = 0x1012}, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    AssertEq(results.list.count, 0);
    AssertTrue(test_backend.read_call_count > 0);

    test_backend.read_call_count = 0;
    options.range.end = options.range.start;
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    AssertEq(results.list.count, 0);
    AssertEq(test_backend.read_call_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanFindsChunkBoundaryMatchesAndHonorsLimit)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 boundary[] = {0xde, 0xad, 0xbe};
    memcpy(test_backend.memory + 3, boundary, sizeof(boundary));
    test_backend.memory[0x20] = 0xfa;
    test_backend.memory[0x22] = 0xfa;
    test_backend.memory[0x24] = 0xfa;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "de ad be", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr boundary_expected[] = {0x1003};
    Test_AssertScanAddresses(&results, boundary_expected, ArrayCount(boundary_expected));

    Test_ParsePattern(arena, "fa", &pattern);
    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1028}, .limit = 2, .chunk_size = 3};
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr limit_expected[] = {0x1020, 0x1022};
    Test_AssertScanAddresses(&results, limit_expected, ArrayCount(limit_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanUsesRegionIntersectionWhenAvailable)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x10] = 0xcc;
    test_backend.memory[0x22] = 0xcc;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 8};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1022};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanFindsPatternAcrossAdjacentReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    U8 bytes[] = {0xde, 0xad, 0xbe, 0xef};
    memcpy(test_backend.memory + 0x2e, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x4e, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "de ad be ef", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 0x10};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x102e};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanDirectReadsWithoutListRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_DisableListRegions(&test_backend);
    test_backend.region_count = 0;
    test_backend.memory[0x10] = 0xcc;
    test_backend.memory[0x22] = 0xcc;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 8};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1010, 0x1022};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanSkipsUnreadableHolesAndReportsFullyUnreadableRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_DisableListRegions(&test_backend);
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1040, 0x1050);
    test_backend.memory[0x32] = 0xab;
    test_backend.memory[0x52] = 0xab;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ab", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1030, .end = 0x1060}, .chunk_size = 16};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1032, 0x1052};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    test_backend.unreadable_range_count = 0;
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1030, 0x1060);
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Unreadable);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanScansPartialReads)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_DisableListRegions(&test_backend);
    Test_MemmyBackend_SetReadLimit(&test_backend, 3);
    U8 bytes[] = {0x80, 0x81};
    memcpy(test_backend.memory + 1, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "80 81", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1001};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanSkipsNonReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1010, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Free);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Reserved);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read | Memmy_RegionAccess_Guard,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x10] = 0xee;
    test_backend.memory[0x20] = 0xee;
    test_backend.memory[0x30] = 0xee;
    test_backend.memory[0x40] = 0xee;
    test_backend.memory[0x50] = 0xee;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ee", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1060}, .chunk_size = 8};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1050};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanFindsScalarValuesAtMultipleAlignments)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 scalar[] = {0x34, 0x12};
    memcpy(test_backend.memory + 1, scalar, sizeof(scalar));
    memcpy(test_backend.memory + 4, scalar, sizeof(scalar));
    memcpy(test_backend.memory + 7, scalar, sizeof(scalar));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Value value = {0};
    Test_ParseValue(arena, "u16", Memmy_PointerWidth_64, "0x1234", &value);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x100a}, .chunk_size = 3};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1001, 0x1004, 0x1007};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanPointerWidthAware)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 ptr32[] = {0x44, 0x33, 0x22, 0x11};
    U8 ptr64[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Memmy_Value value = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 5};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    memcpy(test_backend.memory + 0x20, ptr32, sizeof(ptr32));
    Test_OpenProcess(arena, &process);
    Test_ParseValue(arena, "ptr", process->pointer_width, "0x11223344", &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected32[] = {0x1020};
    Test_AssertScanAddresses(&results, expected32, ArrayCount(expected32));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;
    memcpy(test_backend.memory + 0x30, ptr64, sizeof(ptr64));
    Test_OpenProcess(arena, &process);
    Test_ParseValue(arena, "ptr", process->pointer_width, "0x1122334455667788", &value);
    options.range.start = 0x1030;
    options.range.end = 0x1040;
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected64[] = {0x1030};
    Test_AssertScanAddresses(&results, expected64, ArrayCount(expected64));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanBytesUtf8AndUtf16)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    U8 wstr[] = {'A', 0, 'z', 0};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, "Az", 2);
    memcpy(test_backend.memory + 0x40, "az", 2);
    memcpy(test_backend.memory + 0x50, wstr, sizeof(wstr));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Value value = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 3};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    Test_ParseValue(arena, "bytes", Memmy_PointerWidth_64, "48 8b", &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr bytes_expected[] = {0x1020};
    Test_AssertScanAddresses(&results, bytes_expected, ArrayCount(bytes_expected));

    Test_ParseValue(arena, "str", Memmy_PointerWidth_64, "Az", &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr str_expected[] = {0x1030};
    Test_AssertScanAddresses(&results, str_expected, ArrayCount(str_expected));

    Test_ParseValue(arena, "wstr", Memmy_PointerWidth_64, "Az", &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr wstr_expected[] = {0x1050};
    Test_AssertScanAddresses(&results, wstr_expected, ArrayCount(wstr_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanRangeChunkLimitRegionAndReadErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0xde, 0xad, 0xbe};
    memcpy(test_backend.memory + 3, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Value value = {0};
    Test_ParseValue(arena, "bytes", Memmy_PointerWidth_64, "de ad be", &value);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr boundary_expected[] = {0x1003};
    Test_AssertScanAddresses(&results, boundary_expected, ArrayCount(boundary_expected));

    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .limit = 1, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr limit_expected[] = {0x1020};
    Test_AssertScanAddresses(&results, limit_expected, ArrayCount(limit_expected));

    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr region_expected[] = {0x1030};
    Test_AssertScanAddresses(&results, region_expected, ArrayCount(region_expected));

    Test_DisableListRegions(&test_backend);
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1028, 0x1030);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr hole_expected[] = {0x1020, 0x1030};
    Test_AssertScanAddresses(&results, hole_expected, ArrayCount(hole_expected));

    test_backend.unreadable_range_count = 0;
    Test_MemmyBackend_SetReadLimit(&test_backend, 4);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Test_AssertScanAddresses(&results, hole_expected, ArrayCount(hole_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanFindsValueAcrossAdjacentReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    U8 bytes[] = {0xca, 0xfe, 0xba, 0xbe};
    memcpy(test_backend.memory + 0x2e, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x4e, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Value value = {0};
    Test_ParseValue(arena, "bytes", Memmy_PointerWidth_64, "ca fe ba be", &value);
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 0x10};
    Memmy_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x102e};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyContextSetPushPop)
{
    Memmy_Context ctx_a = {0};
    Memmy_Context ctx_b = {0};

    Memmy_Context_Set(&ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_a);

    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx_b);
    AssertTrue(old_ctx == &ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_b);

    Memmy_Context_Pop(old_ctx);
    AssertTrue(Memmy_Context_Get() == &ctx_a);
    Memmy_Context_Set(0);
}

Test(Test_MemmyDispatchRejectsMissingContextAndBackend)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ProcessList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(0);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context ctx = {0};
    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyDispatchRejectsMissingCallback)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Backend backend = {.name = String8_Lit("empty")};
    Memmy_Context ctx = {.backend = &backend};
    Memmy_ProcessList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Process process = {.backend = &backend};
    U8 buffer[4] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(&process, 0, buffer, sizeof(buffer), &bytes_read, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    U64 bytes_written = 0;
    AssertEq(Memmy_Process_Write(&process, 0, buffer, sizeof(buffer), &bytes_written, &error),
             Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCloseMarksProcessClosedWithoutCallback)
{
    Memmy_Backend backend = {.name = String8_Lit("no-close")};
    Memmy_Process process = {.backend = &backend};

    AssertTrue(Memmy_Process_IsOpen(&process));
    Memmy_Process_Close(&process);
    AssertTrue(!Memmy_Process_IsOpen(&process));
    Memmy_Process_Close(&process);
    AssertTrue(!Memmy_Process_IsOpen(&process));
}

Test(Test_MemmyTestBackendCapabilitiesAndReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Backend *backend = Test_MemmyBackend_AsBackend(&test_backend);
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_Read));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_Write));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListProcs));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListModules));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListRegions));

    Memmy_Context ctx = {.backend = backend};
    Memmy_Context_Set(&ctx);

    Memmy_ProcessList processes = {0};
    AssertEq(Memmy_ListProcesses(arena, &processes, 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 1);
    Memmy_ProcessInfo *info = ContainerOf(processes.list.first, Memmy_ProcessInfo, link);
    AssertEq(info->pid, 4242);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write, &process, 0),
             Memmy_Status_Ok);
    AssertTrue(Memmy_Process_IsOpen(process));
    AssertEq(process->pointer_width, Memmy_PointerWidth_64);

    U8 buffer[4] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(process, test_backend.memory_base + 2, buffer, sizeof(buffer), &bytes_read, 0),
             Memmy_Status_Ok);
    AssertEq(bytes_read, 4);
    AssertEq(buffer[0], 2);
    AssertEq(buffer[3], 5);

    U8 replacement[2] = {99, 100};
    U64 bytes_written = 0;
    AssertEq(
        Memmy_Process_Write(process, test_backend.memory_base + 4, replacement, sizeof(replacement), &bytes_written, 0),
        Memmy_Status_Ok);
    AssertEq(bytes_written, 2);
    AssertEq(test_backend.memory[4], 99);
    AssertEq(test_backend.memory[5], 100);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Memmy_RegionList regions = {0};
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, 0), Memmy_Status_Ok);
    AssertEq(regions.list.count, 1);

    Memmy_Process_Close(process);
    AssertTrue(!Memmy_Process_IsOpen(process));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessReadDispatchAndFailureMapping)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x5000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read, &process, &error), Memmy_Status_Ok);

    U8 buffer[8] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_Ok);
    AssertEq(bytes_read, 4);
    AssertEq(buffer[0], 1);
    AssertEq(buffer[3], 4);

    Test_MemmyBackend_SetReadLimit(&test_backend, 2);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_PartialRead);
    AssertEq(bytes_read, 2);

    Test_MemmyBackend_SetReadLimit(&test_backend, 0);
    AssertEq(Memmy_Process_Read(process, 0x6000, buffer, 4, &bytes_read, &error), Memmy_Status_Unreadable);
    AssertEq(bytes_read, 0);

    Test_MemmyBackend_SetReadStatus(&test_backend, Memmy_Status_AccessDenied);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Test_MemmyBackend_SetReadStatus(&test_backend, Memmy_Status_PlatformError);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_PlatformError);
    AssertEq(error.status, Memmy_Status_PlatformError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessWriteDispatchAndFailureMapping)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x5000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Write, &process, &error), Memmy_Status_Ok);

    U8 replacement[4] = {0xaa, 0xbb, 0xcc, 0xdd};
    U64 bytes_written = 0;
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_Ok);
    AssertEq(bytes_written, 4);
    AssertEq(test_backend.memory[1], 0xaa);
    AssertEq(test_backend.memory[4], 0xdd);

    Test_MemmyBackend_SetWriteLimit(&test_backend, 2);
    replacement[0] = 0x11;
    replacement[1] = 0x22;
    AssertEq(Memmy_Process_Write(process, 0x5008, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_PartialWrite);
    AssertEq(bytes_written, 2);
    AssertEq(test_backend.memory[8], 0x11);
    AssertEq(test_backend.memory[9], 0x22);

    Test_MemmyBackend_SetWriteLimit(&test_backend, 0);
    AssertEq(Memmy_Process_Write(process, 0x6000, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_Unwritable);
    AssertEq(bytes_written, 0);

    Test_MemmyBackend_SetWriteStatus(&test_backend, Memmy_Status_AccessDenied);
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Test_MemmyBackend_SetWriteStatus(&test_backend, Memmy_Status_PlatformError);
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_PlatformError);
    AssertEq(error.status, Memmy_Status_PlatformError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyTestBackendConfiguredInventory)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit("C:\\alpha.exe"),
                                 Memmy_PointerWidth_32);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"), 0x400000,
                                0x3000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x500000, 0x1000, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_ProcessList processes = {0};
    AssertEq(Memmy_ListProcesses(arena, &processes, 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 2);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 222, Memmy_ProcessAccess_Query, &process, 0), Memmy_Status_Ok);
    AssertEq(process->pointer_width, Memmy_PointerWidth_64);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Memmy_RegionList regions = {0};
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, 0), Memmy_Status_Ok);
    AssertEq(regions.list.count, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 str_bytes[] = {'h', 'e', 'l', 'l', 'o'};
    U8 wstr_bytes[] = {'H', 0, 'i', 0};
    U8 escaped_str_bytes[] = {'a', 0, 'b', '\n', '"', '\\', 0x7f};
    U8 escaped_wstr_bytes[] = {'A', 0, 0, 0, '\n', 0, 'B', 0};
    memcpy(test_backend.memory + 0x40, str_bytes, sizeof(str_bytes));
    memcpy(test_backend.memory + 0x50, wstr_bytes, sizeof(wstr_bytes));
    memcpy(test_backend.memory + 0x60, escaped_str_bytes, sizeof(escaped_str_bytes));
    memcpy(test_backend.memory + 0x70, escaped_wstr_bytes, sizeof(escaped_wstr_bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *u32_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1002", "--type", "u32"};
    char *ptr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1008", "--type", "ptr"};
    char *bytes_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x100a", "--type", "bytes", "--count", "3"};
    char *str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1040", "--type", "str", "--count", "5"};
    char *wstr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1050", "--type", "wstr", "--count", "2"};
    char *escaped_str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1060", "--type", "str", "--count", "7"};
    char *escaped_wstr_argv[] = {"memmy",  "peek",   "--pid", "4242",    "--addr",
                                 "0x1070", "--type", "wstr",  "--count", "4"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(u32_argv), u32_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001002: u32 84148994  0x05040302\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001008: ptr 1084818905618843912  0x0f0e0d0c0b0a0908\n"));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x00001008: ptr 185207048  0x0b0a0908\n"));
    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bytes_argv), bytes_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x000000000000100a: bytes 0a 0b 0c\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001040: str \"hello\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001050: wstr \"Hi\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_str_argv), escaped_str_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001060: str \"a\\0b\\n\\\"\\\\\\x7f\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_wstr_argv), escaped_wstr_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001070: wstr \"A\\0\\nB\"\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekCountAndAddressValidation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *missing_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "bytes"};
    char *extra_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u16", "--count", "2"};
    char *bad_addr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000+4", "--type", "u8"};
    char *bad_str[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x10ff", "--type", "str", "--count", "1"};
    char *bad_wstr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1080", "--type", "wstr", "--count", "1"};

    test_backend.memory[0xff] = 0xff;
    test_backend.memory[0x80] = 0x00;
    test_backend.memory[0x81] = 0xd8;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(missing_count), missing_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(extra_count), extra_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_addr), bad_addr, &out, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_str), bad_str, &out, &error),
             Memmy_Status_InvalidEncoding);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_wstr), bad_wstr, &out, &error),
             Memmy_Status_InvalidEncoding);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeDryRunLeavesMemoryUnchanged)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    U8 before[4] = {0};
    memcpy(before, test_backend.memory + 4, sizeof(before));

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy",  "poke", "--pid",   "4242", "--addr",   "0x1004",
                    "--type", "u32",  "--value", "1337", "--dry-run"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("would write:\n"
                                 "  process: 4242\n"
                                 "  address: 0x0000000000001004\n"
                                 "  type:    u32\n"
                                 "  old:     117835012  0x07060504\n"
                                 "  new:     1337  0x00000539\n"));
    AssertEq(memcmp(before, test_backend.memory + 4, sizeof(before)), 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeWritesRepresentativeValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *u32_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1020", "--type", "u32", "--value", "1337"};
    char *ptr_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                        "0x1030", "--type", "ptr",   "--value", "0x1122334455667788"};
    char *bytes_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                          "0x1040", "--type", "bytes", "--value", "90 90 cc"};
    char *str_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1050", "--type", "str", "--value", "hello"};
    char *wstr_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1060", "--type", "wstr", "--value", "Hi"};
    char *str_option_value_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                                     "0x1070", "--type", "str",   "--value", "--flag"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(u32_argv), u32_argv, &out, &error), Memmy_Status_Ok);
    U8 u32_expected[] = {0x39, 0x05, 0x00, 0x00};
    AssertEq(memcmp(test_backend.memory + 0x20, u32_expected, sizeof(u32_expected)), 0);
    AssertTrue(String8_Find(out, String8_Lit("wrote:\n"), 0) != STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    U8 ptr_expected[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    AssertEq(memcmp(test_backend.memory + 0x30, ptr_expected, sizeof(ptr_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bytes_argv), bytes_argv, &out, &error), Memmy_Status_Ok);
    U8 bytes_expected[] = {0x90, 0x90, 0xcc};
    AssertEq(memcmp(test_backend.memory + 0x40, bytes_expected, sizeof(bytes_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(memcmp(test_backend.memory + 0x50, "hello", 5), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    U8 wstr_expected[] = {'H', 0, 'i', 0};
    AssertEq(memcmp(test_backend.memory + 0x60, wstr_expected, sizeof(wstr_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_option_value_argv), str_option_value_argv, &out, &error),
             Memmy_Status_Ok);
    AssertEq(memcmp(test_backend.memory + 0x70, "--flag", 6), 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeValidation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *bad_addr[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1000+4", "--type", "u8", "--value", "1"};
    char *missing_value[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1000", "--type", "u8"};
    char *count[] = {"memmy",  "poke",  "--pid",   "4242", "--addr",  "0x1000",
                     "--type", "bytes", "--value", "90",   "--count", "1"};
    char *bad_old_str[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x10f0", "--type", "str", "--value", "ok"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_addr), bad_addr, &out, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(missing_value), missing_value, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(count), count, &out, &error), Memmy_Status_ParseError);

    test_backend.memory[0xf0] = 0xff;
    test_backend.memory[0xf1] = 0xff;
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_old_str), bad_old_str, &out, &error),
             Memmy_Status_InvalidEncoding);
    AssertEq(test_backend.memory[0xf0], 0xff);
    AssertEq(test_backend.memory[0xf1], 0xff);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPscanTextOutputRangeFormsAndWildcard)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b, 0x11, 0x89};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *end_argv[] = {"memmy",  "pscan", "--pid",  "4242",      "--start",
                        "0x1020", "--end", "0x1030", "--pattern", "48 8b ?? 89"};
    char *length_argv[] = {"memmy",  "pscan",    "--pid", "4242",      "--start",
                           "0x1020", "--length", "0x20",  "--pattern", "48 8b ?? 89"};
    char *jsonl_argv[] = {"memmy",    "pscan", "--pid",     "4242",        "--start", "0x1020",
                          "--length", "0x20",  "--pattern", "48 8b ?? 89", "--jsonl"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(end_argv), end_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(length_argv), length_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n0x0000000000001030\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(jsonl_argv), jsonl_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\"}\n"
                                 "{\"address\":\"0x0000000000001030\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliScanTextOutputRangeFormsAndValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *end_argv[] = {"memmy", "scan",   "--pid",  "4242",  "--start", "0x1020",
                        "--end", "0x1030", "--type", "bytes", "--value", "48 8b"};
    char *length_argv[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1020",  "--length",
                           "0x20",  "--type", "bytes", "--value", "48 8b",   "--limit", "1"};
    char *jsonl_argv[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1020", "--length",
                          "0x20",  "--type", "bytes", "--value", "48 8b",   "--jsonl"};
    char *wildcard_argv[] = {"memmy", "scan",   "--pid",  "4242",  "--start", "0x1020",
                             "--end", "0x1040", "--type", "bytes", "--value", "48 ??"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(end_argv), end_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(length_argv), length_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(jsonl_argv), jsonl_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\"}\n"
                                 "{\"address\":\"0x0000000000001030\"}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wildcard_argv), wildcard_argv, &out, &error),
             Memmy_Status_ParseError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliRejectsPokeOptionsOnOtherCommands)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_value[] = {"memmy", "procs", "--value", "ignored"};
    char *peek_dry_run[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8", "--dry-run"};
    char *peek_jsonl[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8", "--jsonl"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_value), procs_value, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_dry_run), peek_dry_run, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_jsonl), peek_jsonl, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliHelpAndVersion)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *help_argv[] = {"memmy", "--help"};
    char *version_argv[] = {"memmy", "--version"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(help_argv), help_argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("procs"), 0) != STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(version_argv), version_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("memmy 0.0.0\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcsModsRegionsTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit("C:\\alpha.exe"),
                                 Memmy_PointerWidth_32);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "procs", "--filter", "beta"};
    char *mods_argv[] = {"memmy", "mods", "--pid", "222", "--filter", "beta"};
    char *regions_argv[] = {"memmy", "regions", "--pid", "222"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n222    x64    beta.exe\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                SIZE        NAME\n"
                                 "0x00007ff800000000  0x1a4000    beta.dll\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"
                                 "0x000001d800000000  0x000001d800010000  0x10000     rw-     committed\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliNameAmbiguityAndRegionOverflow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("same.exe"), String8_Lit("C:\\one\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("same.exe"), String8_Lit("C:\\two\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 333, String8_Lit("overflow.exe"), String8_Lit("C:\\overflow.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 333, U64_MAX, 1, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *ambiguous_argv[] = {"memmy", "mods", "--name", "same.exe"};
    char *overflow_argv[] = {"memmy", "regions", "--pid", "333"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ambiguous_argv), ambiguous_argv, &out, &error),
             Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(overflow_argv), overflow_argv, &out, &error),
             Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonHelpers)
{
    Arena *arena = Arena_CreateDefault();
    U8 bytes[] = {0x00, 0x0a, 0xff};

    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_64, 0x4242), String8_Lit("0x0000000000004242"));
    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_32, 0x4242), String8_Lit("0x00004242"));
    AssertStrEq(Memmy_Cli_FormatHexBytes(arena, String8_Make(bytes, ArrayCount(bytes))), String8_Lit("00 0a ff"));
    AssertStrEq(Memmy_Cli_FormatJsonString(arena, String8_Lit("a\0b\n\"\\")), String8_Lit("\"a\\u0000b\\n\\\"\\\\\""));

    Memmy_Error error = {
        .status = Memmy_Status_ParseError,
        .message = String8_Lit("bad \"address\""),
        .context = String8_Lit("address"),
        .input = String8_Lit("0x"),
        .byte_offset = 2,
        .byte_count = 1,
        .os_code = 5,
    };
    AssertStrEq(Memmy_Cli_FormatJsonError(arena, &error),
                String8_Lit("{\"ok\":false,\"error\":{\"status\":\"parse_error\",\"message\":\"bad "
                            "\\\"address\\\"\",\"context\":\"address\",\"input\":\"0x\",\"byte_offset\":2,"
                            "\"byte_count\":1,\"os_code\":5}}\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "--json", "procs"};
    char *mods_argv[] = {"memmy", "--json", "mods", "--name", "beta.exe"};
    char *regions_argv[] = {"memmy", "--json", "regions", "--pid", "222"};
    char *peek_argv[] = {"memmy", "--json", "peek", "--name", "beta.exe", "--addr", "0x1002", "--type", "u32"};
    char *poke_argv[] = {"memmy",  "--json", "poke", "--pid",   "222",    "--addr",
                         "0x1004", "--type", "u16",  "--value", "0x1234", "--dry-run"};
    char *scan_argv[] = {"memmy",    "--json", "scan",   "--pid", "222",     "--start", "0x1020",
                         "--length", "2",      "--type", "bytes", "--value", "48 8b"};
    char *pscan_argv[] = {"memmy",  "--json",   "pscan", "--pid",     "222",  "--start",
                          "0x1020", "--length", "2",     "--pattern", "48 ??"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"pid\":222,\"name\":\"beta.exe\",\"path\":\"C:\\\\beta.exe\","
                                 "\"pointer_width\":64}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x00007ff800000000\",\"size\":\"0x1a4000\","
                                 "\"name\":\"beta.dll\",\"path\":\"C:\\\\beta.dll\"}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x000001d800000000\",\"end\":\"0x000001d800010000\","
                                 "\"size\":\"0x10000\",\"access\":\"rw-\",\"state\":\"committed\"}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001002\",\"type\":\"u32\",\"value\":84148994,"
                                 "\"hex\":\"0x05040302\"}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"process\":222,\"address\":\"0x0000000000001004\",\"type\":\"u16\","
                                 "\"old\":\"1284  0x0504\",\"new\":\"4660  0x1234\",\"dry_run\":true}\n"));

    Test_DisableListRegions(&test_backend);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"results\":[{\"address\":\"0x0000000000001020\"}]}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"results\":[{\"address\":\"0x0000000000001020\"}]}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonNonFiniteFloatValuesAreValidJson)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    U32 f32_nan_bits = 0x7fc00000;
    U64 f64_inf_bits = 0x7ff0000000000000ull;
    memcpy(test_backend.memory + 0x20, &f32_nan_bits, sizeof(f32_nan_bits));
    memcpy(test_backend.memory + 0x30, &f64_inf_bits, sizeof(f64_inf_bits));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *f32_nan_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1020", "--type", "f32"};
    char *f64_inf_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1030", "--type", "f64"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f32_nan_argv), f32_nan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\",\"type\":\"f32\",\"value\":null}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f64_inf_argv), f64_inf_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001030\",\"type\":\"f64\",\"value\":null}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcessAccessRequests)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *mods_argv[] = {"memmy", "mods", "--pid", "4242"};
    char *regions_argv[] = {"memmy", "regions", "--pid", "4242"};
    char *peek_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8"};
    char *scan_argv[] = {"memmy",    "scan", "--pid",  "4242",  "--start", "0x1020",
                         "--length", "2",    "--type", "bytes", "--value", "48 8b"};
    char *pscan_argv[] = {"memmy",  "pscan",    "--pid", "4242",      "--start",
                          "0x1020", "--length", "2",     "--pattern", "48 ??"};
    char *poke_argv[] = {"memmy",  "poke", "--pid",   "4242", "--addr",   "0x1020",
                         "--type", "u8",   "--value", "1",    "--dry-run"};

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Query);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Query);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliInvalidOptionsAndNameNotFound)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *duplicate_json[] = {"memmy", "--json", "--json", "procs"};
    char *unknown_option[] = {"memmy", "procs", "--expr", "module+4"};
    char *name_not_found[] = {"memmy", "peek", "--name", "missing.exe", "--addr", "0x1000", "--type", "u8"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(duplicate_json), duplicate_json, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--json"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(unknown_option), unknown_option, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--expr"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(name_not_found), name_not_found, &out, &error),
             Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("process"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExitCodeMapping)
{
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ok), 0);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_ParseError), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidArgument), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Overflow), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidEncoding), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_NotFound), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ambiguous), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_AccessDenied), 4);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialRead), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialWrite), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unsupported), 6);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PlatformError), 7);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unreadable), 1);
}

Test(Test_MemmyDefaultContextWin32ReadWriteCallbacks)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS
    AssertEq(status, Memmy_Status_Ok);
    AssertTrue(ctx.backend != 0);
    AssertTrue(Memmy_Backend_HasCapability(ctx.backend, Memmy_BackendCap_Read));
    AssertTrue(Memmy_Backend_HasCapability(ctx.backend, Memmy_BackendCap_Write));
    AssertTrue(ctx.backend->read != 0);
    AssertTrue(ctx.backend->write != 0);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

Test(Test_MemmyDefaultContextWin32ReadWriteSelfProcess)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    volatile U32 value = 0x11223344;
    U32 read_value = 0;
    U32 write_value = 0x55667788;
    U64 byte_count = 0;
    Memmy_Process *process = 0;

    AssertEq(Memmy_Process_Open(arena, Os_GetProcessId(), Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write,
                                &process, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Process_Read(process, (Memmy_Addr)(uintptr_t)&value, &read_value, sizeof(read_value), &byte_count,
                                &error),
             Memmy_Status_Ok);
    AssertEq(byte_count, sizeof(read_value));
    AssertEq(read_value, 0x11223344);

    AssertEq(Memmy_Process_Write(process, (Memmy_Addr)(uintptr_t)&value, &write_value, sizeof(write_value), &byte_count,
                                 &error),
             Memmy_Status_Ok);
    AssertEq(byte_count, sizeof(write_value));
    AssertEq(value, 0x55667788);

    Memmy_Process_Close(process);
    Memmy_Context_Pop(old_ctx);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

TestSuite suite_memmy = TestSuite_Make(
    "Memmy", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), TestCase_Make(Test_MemmyStatusAndErrorHelpers),
    TestCase_Make(Test_MemmyTypeParseAcceptsV0Spellings), TestCase_Make(Test_MemmyTypeParseRejectsUnknownNames),
    TestCase_Make(Test_MemmyValueParseIntegerBounds),
    TestCase_Make(Test_MemmyValueParseIntegerRejectsInvalidAndOutOfRange),
    TestCase_Make(Test_MemmyValueParseIntegerRejectsInvalidSyntaxAndWrongSigns),
    TestCase_Make(Test_MemmyValueParsePointerWidths), TestCase_Make(Test_MemmyValueParseFloatsAndText),
    TestCase_Make(Test_MemmyPatternParseBytesWildcardsAndRejection),
    TestCase_Make(Test_MemmyValueParseBytesRejectsWildcards),
    TestCase_Make(Test_MemmyParseAddressAcceptsUnsignedTokens),
    TestCase_Make(Test_MemmyParseAddressRejectsExpressionsAndNames),
    TestCase_Make(Test_MemmyParseAddressRejectsOverflow),
    TestCase_Make(Test_MemmyParseSizeAcceptsDecimalAndHexAndRejectsOverflow),
    TestCase_Make(Test_MemmyRangeFromStartEndValidatesOrder),
    TestCase_Make(Test_MemmyRangeFromStartLengthAllowsZeroAndRejectsOverflow),
    TestCase_Make(Test_MemmyCliParseRangeOptionsAcceptsValidShapes),
    TestCase_Make(Test_MemmyCliParseRangeOptionsRejectsInvalidCombinations), TestCase_Make(Test_MemmyContextSetPushPop),
    TestCase_Make(Test_MemmyDispatchRejectsMissingContextAndBackend),
    TestCase_Make(Test_MemmyDispatchRejectsMissingCallback),
    TestCase_Make(Test_MemmyCloseMarksProcessClosedWithoutCallback),
    TestCase_Make(Test_MemmyTestBackendCapabilitiesAndReadWrite),
    TestCase_Make(Test_MemmyProcessReadDispatchAndFailureMapping),
    TestCase_Make(Test_MemmyProcessWriteDispatchAndFailureMapping),
    TestCase_Make(Test_MemmyTestBackendConfiguredInventory), TestCase_Make(Test_MemmyCliPeekTextOutput),
    TestCase_Make(Test_MemmyCliPeekCountAndAddressValidation), TestCase_Make(Test_MemmyScanFindsBeginningMiddleAndEnd),
    TestCase_Make(Test_MemmyScanDoesNotReadOutsideRequestedRangeAndAllowsZeroLength),
    TestCase_Make(Test_MemmyScanFindsChunkBoundaryMatchesAndHonorsLimit),
    TestCase_Make(Test_MemmyScanUsesRegionIntersectionWhenAvailable),
    TestCase_Make(Test_MemmyScanFindsPatternAcrossAdjacentReadableRegions),
    TestCase_Make(Test_MemmyScanDirectReadsWithoutListRegions),
    TestCase_Make(Test_MemmyScanSkipsUnreadableHolesAndReportsFullyUnreadableRange),
    TestCase_Make(Test_MemmyScanScansPartialReads), TestCase_Make(Test_MemmyScanSkipsNonReadableRegions),
    TestCase_Make(Test_MemmyValueScanFindsScalarValuesAtMultipleAlignments),
    TestCase_Make(Test_MemmyValueScanPointerWidthAware), TestCase_Make(Test_MemmyValueScanBytesUtf8AndUtf16),
    TestCase_Make(Test_MemmyValueScanRangeChunkLimitRegionAndReadErrors),
    TestCase_Make(Test_MemmyValueScanFindsValueAcrossAdjacentReadableRegions),
    TestCase_Make(Test_MemmyCliPokeDryRunLeavesMemoryUnchanged),
    TestCase_Make(Test_MemmyCliPokeWritesRepresentativeValues), TestCase_Make(Test_MemmyCliPokeValidation),
    TestCase_Make(Test_MemmyCliPscanTextOutputRangeFormsAndWildcard),
    TestCase_Make(Test_MemmyCliScanTextOutputRangeFormsAndValues),
    TestCase_Make(Test_MemmyCliRejectsPokeOptionsOnOtherCommands), TestCase_Make(Test_MemmyCliHelpAndVersion),
    TestCase_Make(Test_MemmyCliProcsModsRegionsTextOutput), TestCase_Make(Test_MemmyCliNameAmbiguityAndRegionOverflow),
    TestCase_Make(Test_MemmyCliJsonHelpers), TestCase_Make(Test_MemmyCliJsonSuccessOutput),
    TestCase_Make(Test_MemmyCliJsonNonFiniteFloatValuesAreValidJson), TestCase_Make(Test_MemmyCliProcessAccessRequests),
    TestCase_Make(Test_MemmyCliInvalidOptionsAndNameNotFound), TestCase_Make(Test_MemmyCliExitCodeMapping),
    TestCase_Make(Test_MemmyDefaultContextWin32ReadWriteCallbacks),
    TestCase_Make(Test_MemmyDefaultContextWin32ReadWriteSelfProcess), );
