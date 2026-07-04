// ===========================================================================
// String tests
// ===========================================================================

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"
#include "test_framework.h"

Test(Test_Str8Eq)
{
    AssertTrue(String8_Eq(String8_Lit("hello"), String8_Lit("hello")));
    AssertTrue(!String8_Eq(String8_Lit("hello"), String8_Lit("world")));
    AssertTrue(!String8_Eq(String8_Lit("hello"), String8_Lit("hell")));
    AssertTrue(String8_Eq(String8_Lit(""), String8_Lit("")));
}

Test(Test_Str8EqNocase)
{
    AssertTrue(String8_EqNoCase(String8_Lit("Hello"), String8_Lit("hello")));
    AssertTrue(String8_EqNoCase(String8_Lit("ABC"), String8_Lit("abc")));
    AssertTrue(!String8_EqNoCase(String8_Lit("abc"), String8_Lit("abd")));
}

Test(Test_Str8Cstr)
{
    String8 s = String8_FromCStr("test");
    AssertEq(s.len, 4);
    AssertTrue(String8_Eq(s, String8_Lit("test")));
}

Test(Test_Str8Copy)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8 orig = String8_Lit("hello");
    String8 copy = String8_Copy(a, orig);
    AssertTrue(String8_Eq(orig, copy));
    AssertTrue(copy.data != orig.data);
    Scratch_End(scratch);
}

Test(Test_Str8Pushf)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8 s = String8_PushF(a, "count = %d", 42);
    AssertStrEq(s, String8_Lit("count = 42"));

    String8 s2 = String8_PushF(a, "%s-%s", "foo", "bar");
    AssertStrEq(s2, String8_Lit("foo-bar"));
    Scratch_End(scratch);
}

Test(Test_Str8PrefixSuffix)
{
    String8 s = String8_Lit("hello world");
    AssertStrEq(String8_Prefix(s, 5), String8_Lit("hello"));
    AssertStrEq(String8_Suffix(s, 5), String8_Lit("world"));
    AssertStrEq(String8_Prefix(s, 100), s);
    AssertStrEq(String8_Suffix(s, 100), s);
}

Test(Test_Str8Substr)
{
    String8 s = String8_Lit("hello world");
    AssertStrEq(String8_Substr(s, 6, 5), String8_Lit("world"));
    AssertStrEq(String8_Substr(s, 0, 5), String8_Lit("hello"));
    AssertEq(String8_Substr(s, 100, 5).len, 0);
}

Test(Test_Str8TrimWhitespace)
{
    AssertStrEq(String8_TrimWhitespace(String8_Lit("  hello  ")), String8_Lit("hello"));
    AssertStrEq(String8_TrimWhitespace(String8_Lit("\t\n x \r\n")), String8_Lit("x"));
    AssertEq(String8_TrimWhitespace(String8_Lit("   ")).len, 0);
}

Test(Test_Str8StartsEndsWith)
{
    String8 s = String8_Lit("hello world");
    AssertTrue(String8_StartsWith(s, String8_Lit("hello")));
    AssertTrue(!String8_StartsWith(s, String8_Lit("world")));
    AssertTrue(String8_EndsWith(s, String8_Lit("world")));
    AssertTrue(!String8_EndsWith(s, String8_Lit("hello")));
}

Test(Test_Str8ListJoin)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8List list = {0};
    String8List_Push(a, &list, String8_Lit("a"));
    String8List_Push(a, &list, String8_Lit("b"));
    String8List_Push(a, &list, String8_Lit("c"));

    String8 joined = String8List_Join(a, &list, String8_Lit(", "));
    AssertStrEq(joined, String8_Lit("a, b, c"));
    Scratch_End(scratch);
}

Test(Test_Str8ListJoinEmptySep)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8List list = {0};
    String8List_Push(a, &list, String8_Lit("foo"));
    String8List_Push(a, &list, String8_Lit("bar"));

    String8 joined = String8List_Join(a, &list, (String8){0});
    AssertStrEq(joined, String8_Lit("foobar"));
    Scratch_End(scratch);
}

Test(Test_Str8ToU64)
{
    AssertEq(String8_ToU64(String8_Lit("42"), 10), 42);
    AssertEq(String8_ToU64(String8_Lit("ff"), 16), 255);
    AssertEq(String8_ToU64(String8_Lit("0"), 10), 0);
    AssertEq(String8_ToU64(String8_Lit("101"), 2), 5);
}

Test(Test_Str8ToI64)
{
    AssertEq(String8_ToI64(String8_Lit("-42"), 10), -42);
    AssertEq(String8_ToI64(String8_Lit("+10"), 10), 10);
    AssertEq(String8_ToI64(String8_Lit("0"), 10), 0);
}

Test(Test_Str8ParseF64AcceptsValidNumbers)
{
    F64 value = 0;
    U64 offset = U64_MAX;

    AssertEq(String8_ParseF64(String8_Lit("1.5"), &value, &offset), String8_ParseStatus_Ok);
    AssertTrue(value == 1.5);
    AssertEq(offset, 0);

    AssertEq(String8_ParseF64(String8_Lit("-2.25"), &value, &offset), String8_ParseStatus_Ok);
    AssertTrue(value == -2.25);

    AssertEq(String8_ParseF64(String8_Lit("+1e3"), &value, &offset), String8_ParseStatus_Ok);
    AssertTrue(value == 1000.0);
}

Test(Test_Str8ParseF64RejectsInvalidNumbers)
{
    F64 value = 123.0;
    U64 offset = U64_MAX;
    U8 embedded_nul[] = {'1', 0, 'x'};

    AssertEq(String8_ParseF64(String8_Lit(""), &value, &offset), String8_ParseStatus_Invalid);
    AssertEq(offset, 0);

    AssertEq(String8_ParseF64(String8_Lit("abc"), &value, &offset), String8_ParseStatus_Invalid);
    AssertEq(offset, 0);

    AssertEq(String8_ParseF64(String8_Lit("1x"), &value, &offset), String8_ParseStatus_Invalid);
    AssertEq(offset, 1);
    AssertTrue(value == 123.0);

    AssertEq(String8_ParseF64(String8_Make(embedded_nul, ArrayCount(embedded_nul)), &value, &offset),
             String8_ParseStatus_Invalid);
    AssertEq(offset, 1);
    AssertTrue(value == 123.0);
}

Test(Test_Str8ParseF64RejectsRangeErrors)
{
    F64 value = 123.0;
    U64 offset = 0;

    AssertEq(String8_ParseF64(String8_Lit("1e9999"), &value, &offset), String8_ParseStatus_Overflow);
    AssertEq(offset, 6);
    AssertTrue(value == 123.0);
}

TestSuite suite_string = TestSuite_Make(
    "String", TestCase_Make(Test_Str8Eq), TestCase_Make(Test_Str8EqNocase), TestCase_Make(Test_Str8Cstr),
    TestCase_Make(Test_Str8Copy), TestCase_Make(Test_Str8Pushf), TestCase_Make(Test_Str8PrefixSuffix),
    TestCase_Make(Test_Str8Substr), TestCase_Make(Test_Str8TrimWhitespace), TestCase_Make(Test_Str8StartsEndsWith),
    TestCase_Make(Test_Str8ListJoin), TestCase_Make(Test_Str8ListJoinEmptySep), TestCase_Make(Test_Str8ToU64),
    TestCase_Make(Test_Str8ToI64), TestCase_Make(Test_Str8ParseF64AcceptsValidNumbers),
    TestCase_Make(Test_Str8ParseF64RejectsInvalidNumbers), TestCase_Make(Test_Str8ParseF64RejectsRangeErrors), );
