#include "memmy.h"
#include "test_framework.h"

Test(Test_MemmyHeaderExportsBaseTypes)
{
    U64 value = 42;
    AssertEq(value, 42);
}

TestSuite suite_memmy = TestSuite_Make("Memmy", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), );
