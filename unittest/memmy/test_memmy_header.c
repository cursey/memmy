#include "test_memmy_common.h"

Test(Test_MemmyHeaderExportsBaseTypes)
{
    U64 value = 42;
    AssertEq(value, 42);

    Memmy_Addr addr = 0x1000;
    Memmy_Size size = 16;
    Memmy_ProcessInfoSink process_sink = {0};
    Memmy_ModuleSink module_sink = {0};
    Memmy_RegionSink region_sink = {0};
    AssertEq(addr, 0x1000);
    AssertEq(size, 16);
    AssertTrue(process_sink.callback == 0);
    AssertTrue(module_sink.callback == 0);
    AssertTrue(region_sink.callback == 0);
    AssertEq(Memmy_PatternParseFlag_None, 0);

    char const *status_name = Memmy_Status_Name(Memmy_Status_Ok);
    AssertTrue(status_name != 0);
}
TestSuite suite_memmy_header = TestSuite_Make("Memmy Header", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), );
