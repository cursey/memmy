#include "test_memmy_common.h"

Test(Test_MemmyCliPokeDryRunLeavesMemoryUnchanged)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    U8 before[4] = {0};
    memcpy(before, test_backend.memory + 4, sizeof(before));

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy",  "poke", "--pid",   "4242", "--addr",   "0x1004",
                    "--type", "u32",  "--value", "1337", "--dry-run"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("would write:\n"
                                 "  process: 4242\n"
                                 "  address: 0x0000000000001004\n"
                                 "  type:    u32\n"
                                 "  old:     117835012  0x07060504\n"
                                 "  new:     1337  0x00000539\n"));
    AssertEq(memcmp(before, test_backend.memory + 4, sizeof(before)), 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeWritesRepresentativeValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *u32_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1020", "--type", "u32", "--value", "1337"};
    char *ptr_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                        "0x1030", "--type", "ptr",   "--value", "0x1122334455667788"};
    char *bytes_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                          "0x1040", "--type", "bytes", "--value", "90 90 cc"};
    char *str_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1050", "--type", "str", "--value", "hello"};
    char *wstr_argv[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1060", "--type", "wstr", "--value", "Hi"};
    char *str_option_value_argv[] = {"memmy",  "poke",   "--pid", "4242",    "--addr",
                                     "0x1070", "--type", "str",   "--value", "--flag"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(u32_argv), u32_argv, &out, &error), Memmy_Status_Ok);
    U8 u32_expected[] = {0x39, 0x05, 0x00, 0x00};
    AssertEq(memcmp(test_backend.memory + 0x20, u32_expected, sizeof(u32_expected)), 0);
    AssertTrue(String8_Find(out, String8_Lit("wrote:\n"), 0) != STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    U8 ptr_expected[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    AssertEq(memcmp(test_backend.memory + 0x30, ptr_expected, sizeof(ptr_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bytes_argv), bytes_argv, &out, &error), Memmy_Status_Ok);
    U8 bytes_expected[] = {0x90, 0x90, 0xcc};
    AssertEq(memcmp(test_backend.memory + 0x40, bytes_expected, sizeof(bytes_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(memcmp(test_backend.memory + 0x50, "hello", 5), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    U8 wstr_expected[] = {'H', 0, 'i', 0};
    AssertEq(memcmp(test_backend.memory + 0x60, wstr_expected, sizeof(wstr_expected)), 0);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_option_value_argv), str_option_value_argv, &out, &error),
             Memmy_Status_Ok);
    AssertEq(memcmp(test_backend.memory + 0x70, "--flag", 6), 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeValidation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *bad_addr[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1000+4", "--type", "u8", "--value", "1"};
    char *missing_value[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x1000", "--type", "u8"};
    char *count[] = {"memmy",  "poke",  "--pid",   "4242", "--addr",  "0x1000",
                     "--type", "bytes", "--value", "90",   "--count", "1"};
    char *bad_old_str[] = {"memmy", "poke", "--pid", "4242", "--addr", "0x10f0", "--type", "str", "--value", "ok"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_addr), bad_addr, &out, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(missing_value), missing_value, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(count), count, &out, &error), Memmy_Status_ParseError);

    test_backend.memory[0xf0] = 0xff;
    test_backend.memory[0xf1] = 0xff;
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_old_str), bad_old_str, &out, &error),
             Memmy_Status_InvalidEncoding);
    AssertEq(test_backend.memory[0xf0], 0xff);
    AssertEq(test_backend.memory[0xf1], 0xff);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPokeJsonSuccessOutput)
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
    char *poke_argv[] = {"memmy",  "--json", "poke", "--pid",   "222",    "--addr",
                         "0x1004", "--type", "u16",  "--value", "0x1234", "--dry-run"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"process\":222,\"address\":\"0x0000000000001004\",\"type\":\"u16\","
                                 "\"old\":\"1284  0x0504\",\"new\":\"4660  0x1234\",\"dry_run\":true}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_poke =
    TestSuite_Make("Memmy CLI poke", TestCase_Make(Test_MemmyCliPokeDryRunLeavesMemoryUnchanged),
                   TestCase_Make(Test_MemmyCliPokeWritesRepresentativeValues),
                   TestCase_Make(Test_MemmyCliPokeValidation), TestCase_Make(Test_MemmyCliPokeJsonSuccessOutput), );
