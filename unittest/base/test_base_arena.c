// ===========================================================================
// Arena tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

Test(Test_ArenaCreateDestroy)
{
    Arena *a = Arena_CreateDefault();
    AssertTrue(a != 0);
    AssertEq(a->cap, Gigabytes(8));
    AssertTrue(a->pos > 0);
    AssertTrue(a->commit >= ARENA_COMMIT_CHUNK);
    Arena_Destroy(a);
}

Test(Test_ArenaPushBasic)
{
    Arena *a = Arena_CreateDefault();
    U64 *p = Arena_PushStruct(a, U64);
    AssertTrue(p != 0);
    AssertEq(*p, 0);
    *p = 42;
    AssertEq(*p, 42);
    Arena_Destroy(a);
}

Test(Test_ArenaPushAlignment)
{
    Arena *a = Arena_CreateDefault();
    Arena_Push(a, 1, 1); // misalign
    U64 *p = Arena_PushStruct(a, U64);
    AssertTrue(((U64)p % _Alignof(U64)) == 0);
    Arena_Destroy(a);
}

Test(Test_ArenaPushZero)
{
    Arena *a = Arena_CreateDefault();
    U8 *buf = Arena_PushArray(a, U8, 256);
    for (U64 i = 0; i < 256; i++)
    {
        AssertEq(buf[i], 0);
    }
    Arena_Destroy(a);
}

Test(Test_ArenaPopTo)
{
    Arena *a = Arena_CreateDefault();
    U64 saved = Arena_Pos(a);
    Arena_PushArray(a, U8, 1024);
    AssertTrue(Arena_Pos(a) > saved);
    Arena_PopTo(a, saved);
    AssertEq(Arena_Pos(a), saved);
    Arena_Destroy(a);
}

Test(Test_ArenaClear)
{
    Arena *a = Arena_CreateDefault();
    U64 initial_pos = Arena_Pos(a);
    Arena_PushArray(a, U8, 4096);
    AssertTrue(Arena_Pos(a) > initial_pos);
    Arena_Clear(a);
    AssertEq(Arena_Pos(a), initial_pos);
    Arena_Destroy(a);
}

Test(Test_ArenaLargeAlloc)
{
    Arena *a = Arena_CreateDefault();
    // Allocate more than one commit chunk to trigger commit growth
    U8 *buf = Arena_PushArray(a, U8, Kilobytes(128));
    AssertTrue(buf != 0);
    AssertTrue(a->commit >= Kilobytes(128));
    // Write to verify the memory is accessible
    buf[0] = 0xAA;
    buf[Kilobytes(128) - 1] = 0xBB;
    AssertEq(buf[0], 0xAA);
    AssertEq(buf[Kilobytes(128) - 1], 0xBB);
    Arena_Destroy(a);
}

Test(Test_ScratchBasic)
{
    Arena *a = Arena_CreateDefault();
    Scratch s = Scratch_Begin(&a, 1);
    AssertTrue(s.arena != 0);
    AssertTrue(s.arena != a);

    U8 *p = Arena_PushArray(s.arena, U8, 128);
    AssertTrue(p != 0);

    Scratch_End(s);
    Arena_Destroy(a);
}

Test(Test_ScratchNoConflict)
{
    Scratch s = Scratch_Begin(0, 0);
    AssertTrue(s.arena != 0);
    U64 *p = Arena_PushStruct(s.arena, U64);
    *p = 99;
    AssertEq(*p, 99);
    Scratch_End(s);
}

Test(Test_ScratchNested)
{
    Arena *a = Arena_CreateDefault();
    Scratch s1 = Scratch_Begin(&a, 1);
    Arena *conflicts[] = {a, s1.arena};
    Scratch s2 = Scratch_Begin(conflicts, 2);
    AssertTrue(s2.arena != s1.arena);
    AssertTrue(s2.arena != a);
    Scratch_End(s2);
    Scratch_End(s1);
    Arena_Destroy(a);
}

Test(Test_ScratchRestoresPos)
{
    Scratch s = Scratch_Begin(0, 0);
    U64 pos_before = Arena_Pos(s.arena);
    Arena_PushArray(s.arena, U8, 4096);
    Scratch_End(s);

    Scratch s2 = Scratch_Begin(0, 0);
    AssertEq(Arena_Pos(s2.arena), pos_before);
    Scratch_End(s2);
}

TestSuite suite_arena = TestSuite_Make(
    "Arena", TestCase_Make(Test_ArenaCreateDestroy), TestCase_Make(Test_ArenaPushBasic),
    TestCase_Make(Test_ArenaPushAlignment), TestCase_Make(Test_ArenaPushZero), TestCase_Make(Test_ArenaPopTo),
    TestCase_Make(Test_ArenaClear), TestCase_Make(Test_ArenaLargeAlloc), TestCase_Make(Test_ScratchBasic),
    TestCase_Make(Test_ScratchNoConflict), TestCase_Make(Test_ScratchNested), TestCase_Make(Test_ScratchRestoresPos), );
