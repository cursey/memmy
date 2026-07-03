// ===========================================================================
// BitSet tests
// ===========================================================================

#include "base_arena.h"
#include "base_bitset.h"
#include "base_core.h"
#include "test_framework.h"

Test(Test_BitsetSetTestClear)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet bs = BitSet_Alloc(a, 256);
    AssertTrue(!BitSet_Test(&bs, 0));
    AssertTrue(!BitSet_Test(&bs, 100));

    BitSet_Set(&bs, 0);
    BitSet_Set(&bs, 63);
    BitSet_Set(&bs, 64);
    BitSet_Set(&bs, 255);

    AssertTrue(BitSet_Test(&bs, 0));
    AssertTrue(BitSet_Test(&bs, 63));
    AssertTrue(BitSet_Test(&bs, 64));
    AssertTrue(BitSet_Test(&bs, 255));
    AssertTrue(!BitSet_Test(&bs, 1));
    AssertTrue(!BitSet_Test(&bs, 128));

    BitSet_Clear(&bs, 63);
    AssertTrue(!BitSet_Test(&bs, 63));
    Scratch_End(scratch);
}

Test(Test_BitsetPopcount)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet bs = BitSet_Alloc(a, 128);
    AssertEq(BitSet_Popcount(&bs), 0);

    BitSet_Set(&bs, 0);
    BitSet_Set(&bs, 64);
    BitSet_Set(&bs, 127);
    AssertEq(BitSet_Popcount(&bs), 3);
    Scratch_End(scratch);
}

Test(Test_BitsetUnion)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet a_bs = BitSet_Alloc(a, 128);
    BitSet b_bs = BitSet_Alloc(a, 128);

    BitSet_Set(&a_bs, 0);
    BitSet_Set(&a_bs, 2);
    BitSet_Set(&b_bs, 1);
    BitSet_Set(&b_bs, 2);

    BitSet_Union(&a_bs, &b_bs);
    AssertTrue(BitSet_Test(&a_bs, 0));
    AssertTrue(BitSet_Test(&a_bs, 1));
    AssertTrue(BitSet_Test(&a_bs, 2));
    AssertEq(BitSet_Popcount(&a_bs), 3);
    Scratch_End(scratch);
}

Test(Test_BitsetIntersect)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet a_bs = BitSet_Alloc(a, 128);
    BitSet b_bs = BitSet_Alloc(a, 128);

    BitSet_Set(&a_bs, 0);
    BitSet_Set(&a_bs, 1);
    BitSet_Set(&a_bs, 2);
    BitSet_Set(&b_bs, 1);
    BitSet_Set(&b_bs, 2);
    BitSet_Set(&b_bs, 3);

    BitSet_Intersect(&a_bs, &b_bs);
    AssertTrue(!BitSet_Test(&a_bs, 0));
    AssertTrue(BitSet_Test(&a_bs, 1));
    AssertTrue(BitSet_Test(&a_bs, 2));
    AssertTrue(!BitSet_Test(&a_bs, 3));
    Scratch_End(scratch);
}

Test(Test_BitsetDifference)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet a_bs = BitSet_Alloc(a, 128);
    BitSet b_bs = BitSet_Alloc(a, 128);

    BitSet_Set(&a_bs, 0);
    BitSet_Set(&a_bs, 1);
    BitSet_Set(&a_bs, 2);
    BitSet_Set(&b_bs, 1);

    BitSet_Difference(&a_bs, &b_bs);
    AssertTrue(BitSet_Test(&a_bs, 0));
    AssertTrue(!BitSet_Test(&a_bs, 1));
    AssertTrue(BitSet_Test(&a_bs, 2));
    Scratch_End(scratch);
}

Test(Test_BitsetNextSet)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet bs = BitSet_Alloc(a, 256);

    BitSet_Set(&bs, 3);
    BitSet_Set(&bs, 67);
    BitSet_Set(&bs, 200);

    AssertEq(BitSet_NextSet(&bs, 0), 3);
    AssertEq(BitSet_NextSet(&bs, 3), 3);
    AssertEq(BitSet_NextSet(&bs, 4), 67);
    AssertEq(BitSet_NextSet(&bs, 68), 200);
    AssertEq(BitSet_NextSet(&bs, 201), U64_MAX);
    Scratch_End(scratch);
}

Test(Test_BitsetClearAll)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    BitSet bs = BitSet_Alloc(a, 128);
    BitSet_Set(&bs, 0);
    BitSet_Set(&bs, 64);
    BitSet_Set(&bs, 127);
    AssertEq(BitSet_Popcount(&bs), 3);
    BitSet_ClearAll(&bs);
    AssertEq(BitSet_Popcount(&bs), 0);
    Scratch_End(scratch);
}

TestSuite suite_bitset = TestSuite_Make("BitSet", TestCase_Make(Test_BitsetSetTestClear),
                                        TestCase_Make(Test_BitsetPopcount), TestCase_Make(Test_BitsetUnion),
                                        TestCase_Make(Test_BitsetIntersect), TestCase_Make(Test_BitsetDifference),
                                        TestCase_Make(Test_BitsetNextSet), TestCase_Make(Test_BitsetClearAll), );
