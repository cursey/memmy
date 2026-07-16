// ===========================================================================
// Sort tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

static I32 CmpU64(void *a, void *b, void *ctx)
{
    Unused(ctx);
    U64 va = *(U64 *)a;
    U64 vb = *(U64 *)b;
    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    return 0;
}

Test(Test_SortBasic)
{
    U64 data[] = {5, 3, 1, 4, 2};
    Sort(data, 5, sizeof(U64), CmpU64, 0);
    AssertEq(data[0], 1);
    AssertEq(data[1], 2);
    AssertEq(data[2], 3);
    AssertEq(data[3], 4);
    AssertEq(data[4], 5);
}

Test(Test_SortAlreadySorted)
{
    U64 data[] = {1, 2, 3, 4, 5};
    Sort(data, 5, sizeof(U64), CmpU64, 0);
    AssertEq(data[0], 1);
    AssertEq(data[4], 5);
}

Test(Test_SortReverse)
{
    U64 data[] = {5, 4, 3, 2, 1};
    Sort(data, 5, sizeof(U64), CmpU64, 0);
    AssertEq(data[0], 1);
    AssertEq(data[4], 5);
}

Test(Test_SortSingle)
{
    U64 data[] = {42};
    Sort(data, 1, sizeof(U64), CmpU64, 0);
    AssertEq(data[0], 42);
}

Test(Test_SortEmpty)
{
    Sort(0, 0, sizeof(U64), CmpU64, 0);
    // Should not crash
}

Test(Test_SortLarge)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    U64 n = 1000;
    U64 *data = Arena_PushArray(a, U64, n);
    for (U64 i = 0; i < n; i++)
    {
        data[i] = n - i;
    }
    Sort(data, n, sizeof(U64), CmpU64, 0);
    for (U64 i = 0; i < n; i++)
    {
        AssertEq(data[i], i + 1);
    }
    Scratch_End(scratch);
}

Test(Test_SortDuplicates)
{
    U64 data[] = {3, 1, 3, 1, 2, 2};
    Sort(data, 6, sizeof(U64), CmpU64, 0);
    AssertEq(data[0], 1);
    AssertEq(data[1], 1);
    AssertEq(data[2], 2);
    AssertEq(data[3], 2);
    AssertEq(data[4], 3);
    AssertEq(data[5], 3);
}

TestSuite suite_sort =
    TestSuite_Make("Sort", TestCase_Make(Test_SortBasic), TestCase_Make(Test_SortAlreadySorted),
                   TestCase_Make(Test_SortReverse), TestCase_Make(Test_SortSingle), TestCase_Make(Test_SortEmpty),
                   TestCase_Make(Test_SortLarge), TestCase_Make(Test_SortDuplicates), );
