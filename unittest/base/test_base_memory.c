#include "base.h"
#include "test_framework.h"

Test(Test_MemoryCopy)
{
    U8 src[] = {1, 2, 3, 4};
    U8 dst[] = {0, 0, 0, 0};
    Memory_Copy(dst, src, ArrayCount(src));
    AssertTrue(Memory_Equals(dst, src, ArrayCount(src)));
}

Test(Test_MemoryMoveOverlapForward)
{
    U8 data[] = {1, 2, 3, 4, 5};
    U8 expected[] = {1, 1, 2, 3, 4};
    Memory_Move(data + 1, data, 4);
    AssertTrue(Memory_Equals(data, expected, ArrayCount(data)));
}

Test(Test_MemoryMoveOverlapBackward)
{
    U8 data[] = {1, 2, 3, 4, 5};
    U8 expected[] = {2, 3, 4, 5, 5};
    Memory_Move(data, data + 1, 4);
    AssertTrue(Memory_Equals(data, expected, ArrayCount(data)));
}

Test(Test_MemorySet)
{
    U8 data[] = {0, 0, 0, 0};
    U8 expected[] = {0xaa, 0xaa, 0xaa, 0xaa};
    Memory_Set(data, 0xaa, ArrayCount(data));
    AssertTrue(Memory_Equals(data, expected, ArrayCount(data)));
}

Test(Test_MemoryEquals)
{
    U8 a[] = {1, 2, 3};
    U8 b[] = {1, 2, 3};
    U8 c[] = {1, 2, 4};
    AssertTrue(Memory_Equals(a, b, ArrayCount(a)));
    AssertTrue(!Memory_Equals(a, c, ArrayCount(a)));
    AssertTrue(Memory_Equals(0, 0, 0));
}

Test(Test_MemoryZeroLengthOps)
{
    U8 data[] = {1};
    Memory_Copy(0, 0, 0);
    Memory_Move(0, 0, 0);
    Memory_Set(0, 0, 0);
    AssertEq(data[0], 1);
}

TestSuite suite_memory =
    TestSuite_Make("Memory", TestCase_Make(Test_MemoryCopy), TestCase_Make(Test_MemoryMoveOverlapForward),
                   TestCase_Make(Test_MemoryMoveOverlapBackward), TestCase_Make(Test_MemorySet),
                   TestCase_Make(Test_MemoryEquals), TestCase_Make(Test_MemoryZeroLengthOps), );
