#include "test_memmy_common.h"

Test(Test_MemmyHeaderExportsBaseTypes)
{
    U64 value = 42;
    AssertEq(value, 42);

    Memmy_Addr addr = 0x1000;
    Memmy_Size size = 16;
    Memmy_ProcessList processes = {0};
    Memmy_ModuleList modules = {0};
    Memmy_RegionList regions = {0};
    AssertEq(addr, 0x1000);
    AssertEq(size, 16);
    AssertEq(processes.list.count, 0);
    AssertEq(modules.list.count, 0);
    AssertEq(regions.list.count, 0);
}
TestSuite suite_memmy_header = TestSuite_Make("Memmy Header", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), );
