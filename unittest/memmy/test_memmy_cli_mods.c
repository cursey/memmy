#include "test_memmy_common.h"

Test(Test_MemmyCliModsTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *mods_argv[] = {"memmy", "mods", "--pid", "222", "--filter", "beta"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                SIZE        NAME\n"
                                 "0x00007ff800000000  0x1a4000    beta.dll\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliModsNameAmbiguity)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("same.exe"), String8_Lit("C:\\one\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("same.exe"), String8_Lit("C:\\two\\same.exe"),
                                 Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *ambiguous_argv[] = {"memmy", "mods", "--name", "same.exe"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ambiguous_argv), ambiguous_argv, &out, &error),
             Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliModsJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *mods_argv[] = {"memmy", "--json", "mods", "--name", "beta.exe"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x00007ff800000000\",\"size\":\"0x1a4000\","
                                 "\"name\":\"beta.dll\",\"path\":\"C:\\\\beta.dll\"}\n"
                                 "]\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_mods =
    TestSuite_Make("Memmy CLI mods", TestCase_Make(Test_MemmyCliModsTextOutput),
                   TestCase_Make(Test_MemmyCliModsNameAmbiguity), TestCase_Make(Test_MemmyCliModsJsonSuccessOutput), );
