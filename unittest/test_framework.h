#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>

#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// Test framework
// ---------------------------------------------------------------------------

typedef void (*TestFn)(void);

typedef struct TestCase TestCase;
struct TestCase
{
    char *name;
    TestFn fn;
};

typedef struct TestSuite TestSuite;
struct TestSuite
{
    char *name;
    TestCase *cases;
    U64 count;
};

extern U64 test__fail_count;
extern U64 test__total_pass;
extern U64 test__total_fail;

void Test_RunSuite(TestSuite *suite);
void Test_RunAll(TestSuite *suites, U64 count);
void Test_ListAll(TestSuite *suites, U64 count);
B32 Test_RunCase(TestSuite *suites, U64 count, char *name);

#define Test(name) static void name(void)

#define AssertTrue(expr)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr);                                         \
            test__fail_count++;                                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define AssertEq(a, b)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        I64 a_ = (I64)(a);                                                                                             \
        I64 b_ = (I64)(b);                                                                                             \
        if (a_ != b_)                                                                                                  \
        {                                                                                                              \
            fprintf(stderr,                                                                                            \
                    "  FAIL: %s:%d: %s == %s "                                                                         \
                    "(%lld != %lld)\n",                                                                                \
                    __FILE__, __LINE__, #a, #b, (long long)a_, (long long)b_);                                         \
            test__fail_count++;                                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define AssertStrEq(a, b)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        String8 a_ = (a);                                                                                              \
        String8 b_ = (b);                                                                                              \
        if (!String8_Eq(a_, b_))                                                                                       \
        {                                                                                                              \
            fprintf(stderr, "  FAIL: %s:%d: \"%.*s\" != \"%.*s\"\n", __FILE__, __LINE__, (int)a_.len, (char *)a_.data, \
                    (int)b_.len, (char *)b_.data);                                                                     \
            test__fail_count++;                                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define TestCase_Make(f) {.name = #f, .fn = (f)}

#define TestSuite_Make(name_str, ...)                                                                                  \
    {.name = (name_str),                                                                                               \
     .cases = (TestCase[]){__VA_ARGS__},                                                                               \
     .count = sizeof((TestCase[]){__VA_ARGS__}) / sizeof(TestCase)}

#endif // TEST_FRAMEWORK_H
