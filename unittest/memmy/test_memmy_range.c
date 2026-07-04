#include "test_memmy_common.h"

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

TestSuite suite_memmy_range = TestSuite_Make("Memmy Range", TestCase_Make(Test_MemmyParseAddressAcceptsUnsignedTokens),
                                             TestCase_Make(Test_MemmyParseAddressRejectsExpressionsAndNames),
                                             TestCase_Make(Test_MemmyParseAddressRejectsOverflow),
                                             TestCase_Make(Test_MemmyParseSizeAcceptsDecimalAndHexAndRejectsOverflow),
                                             TestCase_Make(Test_MemmyRangeFromStartEndValidatesOrder),
                                             TestCase_Make(Test_MemmyRangeFromStartLengthAllowsZeroAndRejectsOverflow),
                                             TestCase_Make(Test_MemmyCliParseRangeOptionsAcceptsValidShapes),
                                             TestCase_Make(Test_MemmyCliParseRangeOptionsRejectsInvalidCombinations), );
