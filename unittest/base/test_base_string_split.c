// ===========================================================================
// String splitting / searching tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

Test(Test_Str8Find)
{
    AssertEq(String8_Find(String8_Lit("hello world"), String8_Lit("world"), 0), 6);
    AssertEq(String8_Find(String8_Lit("hello world"), String8_Lit("hello"), 0), 0);
    AssertEq(String8_Find(String8_Lit("abcabc"), String8_Lit("abc"), 1), 3);
    AssertTrue(String8_Find(String8_Lit("abc"), String8_Lit("xyz"), 0) == STRING8_NPOS);
    AssertTrue(String8_Find(String8_Lit("a"), String8_Lit("ab"), 0) == STRING8_NPOS);
}

Test(Test_Str8FindChar)
{
    AssertEq(String8_FindChar(String8_Lit("hello"), 'l', 0), 2);
    AssertEq(String8_FindChar(String8_Lit("hello"), 'l', 3), 3);
    AssertTrue(String8_FindChar(String8_Lit("hello"), 'z', 0) == STRING8_NPOS);
}

Test(Test_Str8FindLastChar)
{
    AssertEq(String8_FindLastChar(String8_Lit("a/b/c"), '/'), 3);
    AssertEq(String8_FindLastChar(String8_Lit("hello"), 'l'), 3);
    AssertTrue(String8_FindLastChar(String8_Lit("abc"), 'z') == STRING8_NPOS);
}

Test(Test_Str8SplitLines)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;

    String8Slice s = String8_SplitLines(a, String8_Lit("one\ntwo\nthree"));
    AssertEq(s.count, 3);
    AssertStrEq(s.v[0], String8_Lit("one"));
    AssertStrEq(s.v[1], String8_Lit("two"));
    AssertStrEq(s.v[2], String8_Lit("three"));

    String8Slice crlf = String8_SplitLines(a, String8_Lit("a\r\nb\r\nc"));
    AssertEq(crlf.count, 3);
    AssertStrEq(crlf.v[0], String8_Lit("a"));
    AssertStrEq(crlf.v[1], String8_Lit("b"));
    AssertStrEq(crlf.v[2], String8_Lit("c"));

    String8Slice trailing = String8_SplitLines(a, String8_Lit("a\nb\n"));
    AssertEq(trailing.count, 2);

    Scratch_End(scratch);
}

Test(Test_Str8SplitChar)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;

    String8Slice s = String8_SplitChar(a, String8_Lit("a,b,c"), ',');
    AssertEq(s.count, 3);
    AssertStrEq(s.v[0], String8_Lit("a"));
    AssertStrEq(s.v[1], String8_Lit("b"));
    AssertStrEq(s.v[2], String8_Lit("c"));

    String8Slice empty = String8_SplitChar(a, String8_Lit(",,"), ',');
    AssertEq(empty.count, 3);
    AssertEq(empty.v[0].len, 0);
    AssertEq(empty.v[1].len, 0);
    AssertEq(empty.v[2].len, 0);

    Scratch_End(scratch);
}

Test(Test_Str8Split)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;

    String8Slice s = String8_Split(a, String8_Lit("a::b::c"), String8_Lit("::"));
    AssertEq(s.count, 3);
    AssertStrEq(s.v[0], String8_Lit("a"));
    AssertStrEq(s.v[1], String8_Lit("b"));
    AssertStrEq(s.v[2], String8_Lit("c"));

    Scratch_End(scratch);
}

Test(Test_Str8Replace)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;

    String8 r = String8_Replace(a, String8_Lit("foo-bar-baz"), String8_Lit("-"), String8_Lit("_"));
    AssertStrEq(r, String8_Lit("foo_bar_baz"));

    String8 r2 = String8_Replace(a, String8_Lit("xxoxxoxx"), String8_Lit("xx"), String8_Lit("yyy"));
    AssertStrEq(r2, String8_Lit("yyyoyyyoyyy"));

    Scratch_End(scratch);
}

TestSuite suite_string_split =
    TestSuite_Make("String_Split", TestCase_Make(Test_Str8Find), TestCase_Make(Test_Str8FindChar),
                   TestCase_Make(Test_Str8FindLastChar), TestCase_Make(Test_Str8SplitLines),
                   TestCase_Make(Test_Str8SplitChar), TestCase_Make(Test_Str8Split), TestCase_Make(Test_Str8Replace), );
