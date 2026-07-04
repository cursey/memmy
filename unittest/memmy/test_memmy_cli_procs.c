#include "test_memmy_common.h"

Test(Test_MemmyCliProcsTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit("C:\\alpha.exe"),
                                 Memmy_PointerWidth_32);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "procs", "--filter", "beta"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n222    x64    beta.exe\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcsJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "--json", "procs"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"pid\":222,\"name\":\"beta.exe\",\"path\":\"C:\\\\beta.exe\","
                                 "\"pointer_width\":64}\n"
                                 "]\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_procs = TestSuite_Make("Memmy CLI procs", TestCase_Make(Test_MemmyCliProcsTextOutput),
                                                 TestCase_Make(Test_MemmyCliProcsJsonSuccessOutput), );
