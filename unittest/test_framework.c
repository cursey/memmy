#include "test_framework.h"

U64 test__fail_count;
U64 test__total_pass;
U64 test__total_fail;

void Test_RunSuite(TestSuite *suite)
{
    U64 pass = 0;
    U64 fail = 0;
    printf("--- %s ---\n", suite->name);
    for (U64 i = 0; i < suite->count; i++)
    {
        test__fail_count = 0;
        suite->cases[i].fn();
        if (test__fail_count == 0)
        {
            pass++;
            printf("  PASS: %s\n", suite->cases[i].name);
        }
        else
        {
            fail++;
            printf("  FAIL: %s\n", suite->cases[i].name);
        }
    }
    printf("  %llu passed, %llu failed\n\n", (unsigned long long)pass, (unsigned long long)fail);
    test__total_pass += pass;
    test__total_fail += fail;
}

void Test_RunAll(TestSuite *suites, U64 count)
{
    test__total_pass = 0;
    test__total_fail = 0;
    for (U64 i = 0; i < count; i++)
    {
        Test_RunSuite(&suites[i]);
    }
    printf("=== TOTAL: %llu passed, %llu failed ===\n", (unsigned long long)test__total_pass,
           (unsigned long long)test__total_fail);
}
