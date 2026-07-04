#include "test_memmy_common.h"

Test(Test_MemmyCliRegionsTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *regions_argv[] = {"memmy", "regions", "--pid", "222"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"
                                 "0x000001d800000000  0x000001d800010000  0x10000     rw-     committed\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliRegionsOverflow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 333, String8_Lit("overflow.exe"), String8_Lit("C:\\overflow.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 333, U64_MAX, 1, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *overflow_argv[] = {"memmy", "regions", "--pid", "333"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(overflow_argv), overflow_argv, &out, &error),
             Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliRegionsJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *regions_argv[] = {"memmy", "--json", "regions", "--pid", "222"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x000001d800000000\",\"end\":\"0x000001d800010000\","
                                 "\"size\":\"0x10000\",\"access\":\"rw-\",\"state\":\"committed\"}\n"
                                 "]\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_regions =
    TestSuite_Make("Memmy CLI regions", TestCase_Make(Test_MemmyCliRegionsTextOutput),
                   TestCase_Make(Test_MemmyCliRegionsOverflow), TestCase_Make(Test_MemmyCliRegionsJsonSuccessOutput), );
