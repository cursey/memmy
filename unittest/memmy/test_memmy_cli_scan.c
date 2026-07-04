#include "test_memmy_common.h"

Test(Test_MemmyCliScanTextOutputRangeFormsAndValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *end_argv[] = {"memmy", "scan",   "--pid",  "4242",  "--start", "0x1020",
                        "--end", "0x1030", "--type", "bytes", "--value", "48 8b"};
    char *length_argv[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1020",  "--length",
                           "0x20",  "--type", "bytes", "--value", "48 8b",   "--limit", "1"};
    char *jsonl_argv[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1020", "--length",
                          "0x20",  "--type", "bytes", "--value", "48 8b",   "--jsonl"};
    char *wildcard_argv[] = {"memmy", "scan",   "--pid",  "4242",  "--start", "0x1020",
                             "--end", "0x1040", "--type", "bytes", "--value", "48 ??"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(end_argv), end_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(length_argv), length_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(jsonl_argv), jsonl_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\"}\n"
                                 "{\"address\":\"0x0000000000001030\"}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wildcard_argv), wildcard_argv, &out, &error),
             Memmy_Status_ParseError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliScanJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *scan_argv[] = {"memmy",    "--json", "scan",   "--pid", "222",     "--start", "0x1020",
                         "--length", "2",      "--type", "bytes", "--value", "48 8b"};

    Test_DisableListRegions(&test_backend);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"results\":[{\"address\":\"0x0000000000001020\"}]}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_scan =
    TestSuite_Make("Memmy CLI scan", TestCase_Make(Test_MemmyCliScanTextOutputRangeFormsAndValues),
                   TestCase_Make(Test_MemmyCliScanJsonSuccessOutput), );
