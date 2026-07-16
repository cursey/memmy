// ===========================================================================
// Hash tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

Test(Test_HashFnv1aBasic)
{
    String8 a = String8_Lit("hello");
    String8 b = String8_Lit("world");
    U64 ha = Hash_Fnv1a(a.data, a.len);
    U64 hb = Hash_Fnv1a(b.data, b.len);

    AssertTrue(ha != 0);
    AssertTrue(hb != 0);
    AssertTrue(ha != hb);
}

Test(Test_HashFnv1aDeterministic)
{
    String8 s = String8_Lit("test");
    U64 h1 = Hash_Fnv1a(s.data, s.len);
    U64 h2 = Hash_Fnv1a(s.data, s.len);
    AssertEq(h1, h2);
}

Test(Test_HashFnv1aEmpty)
{
    U64 h = Hash_Fnv1a(0, 0);
    AssertTrue(h != 0);
}

Test(Test_HashU64Basic)
{
    U64 h1 = Hash_U64(0);
    U64 h2 = Hash_U64(1);
    U64 h3 = Hash_U64(U64_MAX);

    AssertTrue(h1 != 0);
    AssertTrue(h2 != 0);
    AssertTrue(h3 != 0);
    AssertTrue(h1 != h2);
    AssertTrue(h2 != h3);
}

Test(Test_HashU64Deterministic)
{
    U64 h1 = Hash_U64(42);
    U64 h2 = Hash_U64(42);
    AssertEq(h1, h2);
}

TestSuite suite_hash = TestSuite_Make("Hash", TestCase_Make(Test_HashFnv1aBasic),
                                      TestCase_Make(Test_HashFnv1aDeterministic), TestCase_Make(Test_HashFnv1aEmpty),
                                      TestCase_Make(Test_HashU64Basic), TestCase_Make(Test_HashU64Deterministic), );
