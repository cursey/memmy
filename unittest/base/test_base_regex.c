// ===========================================================================
// Regex tests
// ===========================================================================

#include "base_arena.h"
#include "base_core.h"
#include "base_regex.h"
#include "base_string.h"
#include "test_framework.h"

static B32 Matches(Arena *a, const char *pat, const char *in)
{
    String8 err = {0};
    Regex *re = Regex_Compile(a, String8_FromCStr((char *)pat), &err);
    if (re == 0)
    {
        return 0;
    }
    Regex_Match m;
    return Regex_Find(re, String8_FromCStr((char *)in), 0, &m);
}

Test(Test_RegexLiteral)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "hello", "hello world"));
    AssertTrue(Matches(scratch.arena, "world", "hello world"));
    AssertTrue(!Matches(scratch.arena, "xyz", "hello world"));
    Scratch_End(scratch);
}

Test(Test_RegexAny)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "a.c", "abc"));
    AssertTrue(Matches(scratch.arena, "a.c", "aXc"));
    AssertTrue(!Matches(scratch.arena, "a.c", "ac"));
    Scratch_End(scratch);
}

Test(Test_RegexStar)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "ab*c", "ac"));
    AssertTrue(Matches(scratch.arena, "ab*c", "abc"));
    AssertTrue(Matches(scratch.arena, "ab*c", "abbbbbc"));
    AssertTrue(Matches(scratch.arena, ".*end", "start middle end"));
    Scratch_End(scratch);
}

Test(Test_RegexPlus)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(!Matches(scratch.arena, "ab+c", "ac"));
    AssertTrue(Matches(scratch.arena, "ab+c", "abc"));
    AssertTrue(Matches(scratch.arena, "ab+c", "abbbc"));
    Scratch_End(scratch);
}

Test(Test_RegexOpt)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "colou?r", "color"));
    AssertTrue(Matches(scratch.arena, "colou?r", "colour"));
    Scratch_End(scratch);
}

Test(Test_RegexClass)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "[0-9]+", "xx42yy"));
    AssertTrue(!Matches(scratch.arena, "[0-9]+", "nodigits"));
    AssertTrue(Matches(scratch.arena, "[^a-z]+", "ABC"));
    AssertTrue(Matches(scratch.arena, "[abc]", "b"));
    Scratch_End(scratch);
}

Test(Test_RegexEscapes)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "\\d+", "abc123"));
    AssertTrue(Matches(scratch.arena, "\\w+", "hello"));
    AssertTrue(Matches(scratch.arena, "\\s+", "a b"));
    AssertTrue(Matches(scratch.arena, "\\.", "a.b"));
    AssertTrue(!Matches(scratch.arena, "\\.", "ab"));
    Scratch_End(scratch);
}

Test(Test_RegexAlt)
{
    Scratch scratch = Scratch_Begin(0, 0);
    AssertTrue(Matches(scratch.arena, "foo|bar", "xx bar yy"));
    AssertTrue(Matches(scratch.arena, "foo|bar", "xx foo yy"));
    AssertTrue(!Matches(scratch.arena, "foo|bar", "xx baz yy"));
    Scratch_End(scratch);
}

Test(Test_RegexGroups)
{
    Scratch scratch = Scratch_Begin(0, 0);
    String8 err = {0};
    Regex *re = Regex_Compile(scratch.arena, String8_Lit("([a-z]+)=([0-9]+)"), &err);
    AssertTrue(re != 0);
    Regex_Match m;
    AssertTrue(Regex_Find(re, String8_Lit("x: name=42 done"), 0, &m));
    AssertEq(m.group_count, 2);
    String8 g0 = String8_Substr(String8_Lit("x: name=42 done"), m.groups[0].start, m.groups[0].end - m.groups[0].start);
    String8 g1 = String8_Substr(String8_Lit("x: name=42 done"), m.groups[1].start, m.groups[1].end - m.groups[1].start);
    AssertStrEq(g0, String8_Lit("name"));
    AssertStrEq(g1, String8_Lit("42"));
    Scratch_End(scratch);
}

Test(Test_RegexCompileError)
{
    Scratch scratch = Scratch_Begin(0, 0);
    String8 err = {0};
    Regex *re = Regex_Compile(scratch.arena, String8_Lit("(unclosed"), &err);
    AssertTrue(re == 0);
    AssertTrue(err.len > 0);
    Scratch_End(scratch);
}

TestSuite suite_regex =
    TestSuite_Make("Regex", TestCase_Make(Test_RegexLiteral), TestCase_Make(Test_RegexAny),
                   TestCase_Make(Test_RegexStar), TestCase_Make(Test_RegexPlus), TestCase_Make(Test_RegexOpt),
                   TestCase_Make(Test_RegexClass), TestCase_Make(Test_RegexEscapes), TestCase_Make(Test_RegexAlt),
                   TestCase_Make(Test_RegexGroups), TestCase_Make(Test_RegexCompileError), );
