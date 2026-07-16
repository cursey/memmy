// ===========================================================================
// Filesystem tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

Test(Test_PathJoin)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    AssertStrEq(Fs_PathJoin(a, String8_Lit("/a"), String8_Lit("b")), String8_Lit("/a/b"));
    AssertStrEq(Fs_PathJoin(a, String8_Lit("/a/"), String8_Lit("b")), String8_Lit("/a/b"));
    AssertStrEq(Fs_PathJoin(a, String8_Lit("/a"), String8_Lit("/b")), String8_Lit("/a/b"));
    AssertStrEq(Fs_PathJoin(a, String8_Lit(""), String8_Lit("b")), String8_Lit("b"));
    Scratch_End(scratch);
}

Test(Test_PathBasename)
{
    AssertStrEq(Fs_PathBasename(String8_Lit("/a/b/c.txt")), String8_Lit("c.txt"));
    AssertStrEq(Fs_PathBasename(String8_Lit("file")), String8_Lit("file"));
    AssertStrEq(Fs_PathBasename(String8_Lit("/root")), String8_Lit("root"));
}

Test(Test_PathDirname)
{
    AssertStrEq(Fs_PathDirname(String8_Lit("/a/b/c.txt")), String8_Lit("/a/b"));
    AssertStrEq(Fs_PathDirname(String8_Lit("file")), String8_Lit("."));
}

Test(Test_PathExtension)
{
    AssertStrEq(Fs_PathExtension(String8_Lit("foo.txt")), String8_Lit(".txt"));
    AssertStrEq(Fs_PathExtension(String8_Lit("foo.tar.gz")), String8_Lit(".gz"));
    AssertEq(Fs_PathExtension(String8_Lit("noext")).len, 0);
    AssertEq(Fs_PathExtension(String8_Lit(".hidden")).len, 0);
}

Test(Test_FsReadWriteRoundTrip)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8 tmp = Fs_TempFile(a, String8_Lit("fs_test"));
    String8 contents = String8_Lit("hello, fs!\n");
    AssertTrue(Fs_WriteFile(tmp, contents));
    String8 read_back = {0};
    AssertTrue(Fs_ReadFile(a, tmp, &read_back));
    AssertStrEq(read_back, contents);
    AssertTrue(Os_FileDelete(tmp));
    Scratch_End(scratch);
}

Test(Test_FsWalk)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    // Walk the project's unittest/ directory; we know it has .c files.
    String8Slice slice = Fs_Walk(a, String8_Lit("unittest"), String8_Lit(".c"));
    AssertTrue(slice.count > 0);
    // All entries should have .c extension
    for (U64 i = 0; i < slice.count; i++)
    {
        AssertStrEq(Fs_PathExtension(slice.v[i]), String8_Lit(".c"));
    }
    Scratch_End(scratch);
}

TestSuite suite_fs = TestSuite_Make("Fs", TestCase_Make(Test_PathJoin), TestCase_Make(Test_PathBasename),
                                    TestCase_Make(Test_PathDirname), TestCase_Make(Test_PathExtension),
                                    TestCase_Make(Test_FsReadWriteRoundTrip), TestCase_Make(Test_FsWalk), );
