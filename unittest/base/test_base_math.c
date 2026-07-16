#include "base.h"
#include "test_framework.h"

Test(Test_F32IsFinite)
{
    union {
        U32 u;
        F32 f;
    } positive_inf = {.u = 0x7f800000u};
    union {
        U32 u;
        F32 f;
    } negative_inf = {.u = 0xff800000u};
    union {
        U32 u;
        F32 f;
    } nan_value = {.u = 0x7fc00000u};

    AssertTrue(F32_IsFinite(0.0f));
    AssertTrue(F32_IsFinite(-123.5f));
    AssertTrue(!F32_IsFinite(positive_inf.f));
    AssertTrue(!F32_IsFinite(negative_inf.f));
    AssertTrue(!F32_IsFinite(nan_value.f));
}

Test(Test_F64IsFinite)
{
    union {
        U64 u;
        F64 f;
    } positive_inf = {.u = 0x7ff0000000000000ull};
    union {
        U64 u;
        F64 f;
    } negative_inf = {.u = 0xfff0000000000000ull};
    union {
        U64 u;
        F64 f;
    } nan_value = {.u = 0x7ff8000000000000ull};

    AssertTrue(F64_IsFinite(0.0));
    AssertTrue(F64_IsFinite(-123.5));
    AssertTrue(!F64_IsFinite(positive_inf.f));
    AssertTrue(!F64_IsFinite(negative_inf.f));
    AssertTrue(!F64_IsFinite(nan_value.f));
}

TestSuite suite_math = TestSuite_Make("Math", TestCase_Make(Test_F32IsFinite), TestCase_Make(Test_F64IsFinite), );
