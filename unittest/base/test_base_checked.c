// ===========================================================================
// Checked arithmetic tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

Test(Test_AddU64Checked)
{
    U64 out = 0;
    AssertTrue(AddU64Checked(40, 2, &out));
    AssertEq(out, 42);

    out = 123;
    AssertTrue(!AddU64Checked(U64_MAX, 1, &out));
    AssertEq(out, 123);
}

Test(Test_SubU64Checked)
{
    U64 out = 0;
    AssertTrue(SubU64Checked(44, 2, &out));
    AssertEq(out, 42);

    out = 123;
    AssertTrue(!SubU64Checked(0, 1, &out));
    AssertEq(out, 123);
}

Test(Test_AddI64ToU64Checked)
{
    U64 out = 0;
    AssertTrue(AddI64ToU64Checked(40, 2, &out));
    AssertEq(out, 42);

    AssertTrue(AddI64ToU64Checked(44, -2, &out));
    AssertEq(out, 42);

    out = 123;
    AssertTrue(!AddI64ToU64Checked(U64_MAX, 1, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!AddI64ToU64Checked(0, -1, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!AddI64ToU64Checked(0, I64_MIN, &out));
    AssertEq(out, 123);
}

Test(Test_AddI64Checked)
{
    I64 out = 0;
    AssertTrue(AddI64Checked(40, 2, &out));
    AssertEq(out, 42);

    AssertTrue(AddI64Checked(-40, -2, &out));
    AssertEq(out, -42);

    out = 123;
    AssertTrue(!AddI64Checked(I64_MAX, 1, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!AddI64Checked(I64_MIN, -1, &out));
    AssertEq(out, 123);
}

Test(Test_SubI64Checked)
{
    I64 out = 0;
    AssertTrue(SubI64Checked(44, 2, &out));
    AssertEq(out, 42);

    AssertTrue(SubI64Checked(-44, -2, &out));
    AssertEq(out, -42);

    out = 123;
    AssertTrue(!SubI64Checked(I64_MIN, 1, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!SubI64Checked(I64_MAX, -1, &out));
    AssertEq(out, 123);
}

Test(Test_MulI64Checked)
{
    I64 out = 0;
    AssertTrue(MulI64Checked(6, 7, &out));
    AssertEq(out, 42);

    AssertTrue(MulI64Checked(-6, 7, &out));
    AssertEq(out, -42);

    AssertTrue(MulI64Checked(0, I64_MIN, &out));
    AssertEq(out, 0);

    out = 123;
    AssertTrue(!MulI64Checked(I64_MAX, 2, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!MulI64Checked(I64_MIN, -1, &out));
    AssertEq(out, 123);
}

Test(Test_DivI64Checked)
{
    I64 out = 0;
    AssertTrue(DivI64Checked(84, 2, &out));
    AssertEq(out, 42);

    AssertTrue(DivI64Checked(-84, 2, &out));
    AssertEq(out, -42);

    out = 123;
    AssertTrue(!DivI64Checked(42, 0, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!DivI64Checked(I64_MIN, -1, &out));
    AssertEq(out, 123);
}

Test(Test_ModI64Checked)
{
    I64 out = 0;
    AssertTrue(ModI64Checked(44, 2, &out));
    AssertEq(out, 0);

    AssertTrue(ModI64Checked(43, 2, &out));
    AssertEq(out, 1);

    out = 123;
    AssertTrue(!ModI64Checked(42, 0, &out));
    AssertEq(out, 123);

    out = 123;
    AssertTrue(!ModI64Checked(I64_MIN, -1, &out));
    AssertEq(out, 123);
}

TestSuite suite_checked = TestSuite_Make(
    "Checked", TestCase_Make(Test_AddU64Checked), TestCase_Make(Test_SubU64Checked),
    TestCase_Make(Test_AddI64ToU64Checked), TestCase_Make(Test_AddI64Checked), TestCase_Make(Test_SubI64Checked),
    TestCase_Make(Test_MulI64Checked), TestCase_Make(Test_DivI64Checked), TestCase_Make(Test_ModI64Checked), );
