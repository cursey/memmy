#include "test_memmy_common.h"

Test(Test_MemmyCliPeekTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 str_bytes[] = {'h', 'e', 'l', 'l', 'o'};
    U8 wstr_bytes[] = {'H', 0, 'i', 0};
    U8 escaped_str_bytes[] = {'a', 0, 'b', '\n', '"', '\\', 0x7f};
    U8 escaped_wstr_bytes[] = {'A', 0, 0, 0, '\n', 0, 'B', 0};
    memcpy(test_backend.memory + 0x40, str_bytes, sizeof(str_bytes));
    memcpy(test_backend.memory + 0x50, wstr_bytes, sizeof(wstr_bytes));
    memcpy(test_backend.memory + 0x60, escaped_str_bytes, sizeof(escaped_str_bytes));
    memcpy(test_backend.memory + 0x70, escaped_wstr_bytes, sizeof(escaped_wstr_bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *u32_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1002", "--type", "u32"};
    char *ptr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1008", "--type", "ptr"};
    char *bytes_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x100a", "--type", "bytes", "--count", "3"};
    char *str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1040", "--type", "str", "--count", "5"};
    char *wstr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1050", "--type", "wstr", "--count", "2"};
    char *escaped_str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1060", "--type", "str", "--count", "7"};
    char *escaped_wstr_argv[] = {"memmy",  "peek",   "--pid", "4242",    "--addr",
                                 "0x1070", "--type", "wstr",  "--count", "4"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(u32_argv), u32_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001002: u32 84148994  0x05040302\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001008: ptr 1084818905618843912  0x0f0e0d0c0b0a0908\n"));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x00001008: ptr 185207048  0x0b0a0908\n"));
    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bytes_argv), bytes_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x000000000000100a: bytes 0a 0b 0c\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001040: str \"hello\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001050: wstr \"Hi\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_str_argv), escaped_str_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001060: str \"a\\0b\\n\\\"\\\\\\x7f\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_wstr_argv), escaped_wstr_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001070: wstr \"A\\0\\nB\"\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekCountAndAddressValidation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *missing_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "bytes"};
    char *extra_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u16", "--count", "2"};
    char *bad_addr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000+4", "--type", "u8"};
    char *bad_str[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x10ff", "--type", "str", "--count", "1"};
    char *bad_wstr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1080", "--type", "wstr", "--count", "1"};

    test_backend.memory[0xff] = 0xff;
    test_backend.memory[0x80] = 0x00;
    test_backend.memory[0x81] = 0xd8;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(missing_count), missing_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(extra_count), extra_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_addr), bad_addr, &out, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_str), bad_str, &out, &error),
             Memmy_Status_InvalidEncoding);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_wstr), bad_wstr, &out, &error),
             Memmy_Status_InvalidEncoding);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

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

Test(Test_MemmyCliPscanTextOutputRangeFormsAndWildcard)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b, 0x11, 0x89};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *end_argv[] = {"memmy",  "pscan", "--pid",  "4242",      "--start",
                        "0x1020", "--end", "0x1030", "--pattern", "48 8b ?? 89"};
    char *length_argv[] = {"memmy",  "pscan",    "--pid", "4242",      "--start",
                           "0x1020", "--length", "0x20",  "--pattern", "48 8b ?? 89"};
    char *jsonl_argv[] = {"memmy",    "pscan", "--pid",     "4242",        "--start", "0x1020",
                          "--length", "0x20",  "--pattern", "48 8b ?? 89", "--jsonl"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(end_argv), end_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(length_argv), length_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n0x0000000000001020\n0x0000000000001030\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(jsonl_argv), jsonl_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\"}\n"
                                 "{\"address\":\"0x0000000000001030\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

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

Test(Test_MemmyCliRejectsPokeOptionsOnOtherCommands)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_value[] = {"memmy", "procs", "--value", "ignored"};
    char *peek_dry_run[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8", "--dry-run"};
    char *peek_jsonl[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8", "--jsonl"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_value), procs_value, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_dry_run), peek_dry_run, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_jsonl), peek_jsonl, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliRejectsV0NonGoalSyntax)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *addr_command[] = {"memmy", "addr", "--pid", "4242"};
    char *expr_option[] = {"memmy", "peek", "--pid", "4242", "--expr", "0x1000", "--type", "u8"};
    char *module_addr[] = {"memmy", "peek", "--pid", "4242", "--addr", "client.dll", "--type", "u8"};
    char *pointer_chain[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000->0x8", "--type", "u8"};
    char *range_option[] = {"memmy",          "scan",   "--pid", "4242",    "--range",
                            "0x1000..0x1010", "--type", "u8",    "--value", "1"};
    char *readable_option[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1000",    "--length",
                               "0x10",  "--type", "u8",    "--value", "1",       "--readable"};
    char *writable_option[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1000",    "--length",
                               "0x10",  "--type", "u8",    "--value", "1",       "--writable"};
    char *executable_option[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1000",      "--length",
                                 "0x10",  "--type", "u8",    "--value", "1",       "--executable"};
    char *implicit_scan[] = {"memmy", "scan", "--pid", "4242", "--type", "u8", "--value", "1"};
    char *duplicate_range[] = {"memmy", "pscan",  "--pid",   "4242",   "--start",   "0x1000",
                               "--end", "0x1010", "--start", "0x1020", "--pattern", "90"};

    struct
    {
        char **argv;
        I32 argc;
        Memmy_Status status;
        String8 context;
    } cases[] = {
        {addr_command, (I32)ArrayCount(addr_command), Memmy_Status_ParseError, String8_Lit("cli")},
        {expr_option, (I32)ArrayCount(expr_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {module_addr, (I32)ArrayCount(module_addr), Memmy_Status_ParseError, String8_Lit("address")},
        {pointer_chain, (I32)ArrayCount(pointer_chain), Memmy_Status_ParseError, String8_Lit("address")},
        {range_option, (I32)ArrayCount(range_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {readable_option, (I32)ArrayCount(readable_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {writable_option, (I32)ArrayCount(writable_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {executable_option, (I32)ArrayCount(executable_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {implicit_scan, (I32)ArrayCount(implicit_scan), Memmy_Status_ParseError, String8_Lit("scan")},
        {duplicate_range, (I32)ArrayCount(duplicate_range), Memmy_Status_InvalidArgument, String8_Lit("cli")},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        error = (Memmy_Error){0};
        AssertEq(Memmy_Cli_RunToString(arena, cases[i].argc, cases[i].argv, &out, &error), cases[i].status);
        AssertStrEq(error.context, cases[i].context);
    }

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliHelpAndVersion)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *help_argv[] = {"memmy", "--help"};
    char *version_argv[] = {"memmy", "--version"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(help_argv), help_argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("procs"), 0) != STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(version_argv), version_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("memmy 0.0.0\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcsModsRegionsTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit("C:\\alpha.exe"),
                                 Memmy_PointerWidth_32);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "procs", "--filter", "beta"};
    char *mods_argv[] = {"memmy", "mods", "--pid", "222", "--filter", "beta"};
    char *regions_argv[] = {"memmy", "regions", "--pid", "222"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n222    x64    beta.exe\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                SIZE        NAME\n"
                                 "0x00007ff800000000  0x1a4000    beta.dll\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"
                                 "0x000001d800000000  0x000001d800010000  0x10000     rw-     committed\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliNameAmbiguityAndRegionOverflow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("same.exe"), String8_Lit("C:\\one\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("same.exe"), String8_Lit("C:\\two\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 333, String8_Lit("overflow.exe"), String8_Lit("C:\\overflow.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 333, U64_MAX, 1, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *ambiguous_argv[] = {"memmy", "mods", "--name", "same.exe"};
    char *overflow_argv[] = {"memmy", "regions", "--pid", "333"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ambiguous_argv), ambiguous_argv, &out, &error),
             Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(overflow_argv), overflow_argv, &out, &error),
             Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonHelpers)
{
    Arena *arena = Arena_CreateDefault();
    U8 bytes[] = {0x00, 0x0a, 0xff};
    char *json_flag[] = {"memmy", "--json", "procs"};
    char *jsonl_flag[] = {"memmy", "scan", "--pid", "4242", "--type", "str", "--value", "needle", "--jsonl"};
    char *json_value[] = {"memmy", "scan", "--pid", "4242", "--type", "str", "--value", "--json"};
    char *jsonl_value[] = {"memmy", "scan", "--pid", "4242", "--type", "str", "--value", "--jsonl"};

    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_64, 0x4242), String8_Lit("0x0000000000004242"));
    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_32, 0x4242), String8_Lit("0x00004242"));
    AssertStrEq(Memmy_Cli_FormatHexBytes(arena, String8_Make(bytes, ArrayCount(bytes))), String8_Lit("00 0a ff"));
    AssertStrEq(Memmy_Cli_FormatJsonString(arena, String8_Lit("a\0b\n\"\\")), String8_Lit("\"a\\u0000b\\n\\\"\\\\\""));
    AssertEq(Memmy_Cli_ArgvHasJson((I32)ArrayCount(json_flag), json_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasJsonl((I32)ArrayCount(jsonl_flag), jsonl_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasJson((I32)ArrayCount(json_value), json_value), 0);
    AssertEq(Memmy_Cli_ArgvHasJsonl((I32)ArrayCount(jsonl_value), jsonl_value), 0);

    Memmy_Error error = {
        .status = Memmy_Status_ParseError,
        .message = String8_Lit("bad \"address\""),
        .context = String8_Lit("address"),
        .input = String8_Lit("0x"),
        .byte_offset = 2,
        .byte_count = 1,
        .os_code = 5,
    };
    AssertStrEq(Memmy_Cli_FormatJsonError(arena, &error),
                String8_Lit("{\"ok\":false,\"error\":{\"status\":\"parse_error\",\"message\":\"bad "
                            "\\\"address\\\"\",\"context\":\"address\",\"input\":\"0x\",\"byte_offset\":2,"
                            "\"byte_count\":1,\"os_code\":5}}\n"));
    AssertStrEq(Memmy_Cli_FormatJsonlError(arena, &error),
                String8_Lit("{\"ok\":false,\"error\":{\"status\":\"parse_error\",\"message\":\"bad "
                            "\\\"address\\\"\",\"context\":\"address\",\"input\":\"0x\",\"byte_offset\":2,"
                            "\"byte_count\":1,\"os_code\":5}}\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "--json", "procs"};
    char *mods_argv[] = {"memmy", "--json", "mods", "--name", "beta.exe"};
    char *regions_argv[] = {"memmy", "--json", "regions", "--pid", "222"};
    char *peek_argv[] = {"memmy", "--json", "peek", "--name", "beta.exe", "--addr", "0x1002", "--type", "u32"};
    char *poke_argv[] = {"memmy",  "--json", "poke", "--pid",   "222",    "--addr",
                         "0x1004", "--type", "u16",  "--value", "0x1234", "--dry-run"};
    char *scan_argv[] = {"memmy",    "--json", "scan",   "--pid", "222",     "--start", "0x1020",
                         "--length", "2",      "--type", "bytes", "--value", "48 8b"};
    char *pscan_argv[] = {"memmy",  "--json",   "pscan", "--pid",     "222",  "--start",
                          "0x1020", "--length", "2",     "--pattern", "48 ??"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"pid\":222,\"name\":\"beta.exe\",\"path\":\"C:\\\\beta.exe\","
                                 "\"pointer_width\":64}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x00007ff800000000\",\"size\":\"0x1a4000\","
                                 "\"name\":\"beta.dll\",\"path\":\"C:\\\\beta.dll\"}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[\n"
                                 "  {\"base\":\"0x000001d800000000\",\"end\":\"0x000001d800010000\","
                                 "\"size\":\"0x10000\",\"access\":\"rw-\",\"state\":\"committed\"}\n"
                                 "]\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001002\",\"type\":\"u32\",\"value\":84148994,"
                                 "\"hex\":\"0x05040302\"}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"process\":222,\"address\":\"0x0000000000001004\",\"type\":\"u16\","
                                 "\"old\":\"1284  0x0504\",\"new\":\"4660  0x1234\",\"dry_run\":true}\n"));

    Test_DisableListRegions(&test_backend);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"results\":[{\"address\":\"0x0000000000001020\"}]}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"results\":[{\"address\":\"0x0000000000001020\"}]}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonNonFiniteFloatValuesAreValidJson)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    U32 f32_nan_bits = 0x7fc00000;
    U64 f64_inf_bits = 0x7ff0000000000000ull;
    memcpy(test_backend.memory + 0x20, &f32_nan_bits, sizeof(f32_nan_bits));
    memcpy(test_backend.memory + 0x30, &f64_inf_bits, sizeof(f64_inf_bits));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *f32_nan_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1020", "--type", "f32"};
    char *f64_inf_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1030", "--type", "f64"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f32_nan_argv), f32_nan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\",\"type\":\"f32\",\"value\":null}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f64_inf_argv), f64_inf_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001030\",\"type\":\"f64\",\"value\":null}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcessAccessRequests)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *mods_argv[] = {"memmy", "mods", "--pid", "4242"};
    char *regions_argv[] = {"memmy", "regions", "--pid", "4242"};
    char *peek_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u8"};
    char *scan_argv[] = {"memmy",    "scan", "--pid",  "4242",  "--start", "0x1020",
                         "--length", "2",    "--type", "bytes", "--value", "48 8b"};
    char *pscan_argv[] = {"memmy",  "pscan",    "--pid", "4242",      "--start",
                          "0x1020", "--length", "2",     "--pattern", "48 ??"};
    char *poke_argv[] = {"memmy",  "poke", "--pid",   "4242", "--addr",   "0x1020",
                         "--type", "u8",   "--value", "1",    "--dry-run"};

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Query);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Query);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliInvalidOptionsAndNameNotFound)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *duplicate_json[] = {"memmy", "--json", "--json", "procs"};
    char *unknown_option[] = {"memmy", "procs", "--expr", "module+4"};
    char *name_not_found[] = {"memmy", "peek", "--name", "missing.exe", "--addr", "0x1000", "--type", "u8"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(duplicate_json), duplicate_json, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--json"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(unknown_option), unknown_option, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--expr"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(name_not_found), name_not_found, &out, &error),
             Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("process"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExitCodeMapping)
{
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ok), 0);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_ParseError), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidArgument), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Overflow), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidEncoding), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_NotFound), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ambiguous), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_AccessDenied), 4);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialRead), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialWrite), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unsupported), 6);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PlatformError), 7);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unreadable), 1);
}
TestSuite suite_memmy_cli = TestSuite_Make(
    "Memmy CLI", TestCase_Make(Test_MemmyCliPeekTextOutput), TestCase_Make(Test_MemmyCliPeekCountAndAddressValidation),
    TestCase_Make(Test_MemmyCliPokeDryRunLeavesMemoryUnchanged),
    TestCase_Make(Test_MemmyCliPokeWritesRepresentativeValues), TestCase_Make(Test_MemmyCliPokeValidation),
    TestCase_Make(Test_MemmyCliPscanTextOutputRangeFormsAndWildcard),
    TestCase_Make(Test_MemmyCliScanTextOutputRangeFormsAndValues),
    TestCase_Make(Test_MemmyCliRejectsPokeOptionsOnOtherCommands), TestCase_Make(Test_MemmyCliRejectsV0NonGoalSyntax),
    TestCase_Make(Test_MemmyCliHelpAndVersion), TestCase_Make(Test_MemmyCliProcsModsRegionsTextOutput),
    TestCase_Make(Test_MemmyCliNameAmbiguityAndRegionOverflow), TestCase_Make(Test_MemmyCliJsonHelpers),
    TestCase_Make(Test_MemmyCliJsonSuccessOutput), TestCase_Make(Test_MemmyCliJsonNonFiniteFloatValuesAreValidJson),
    TestCase_Make(Test_MemmyCliProcessAccessRequests), TestCase_Make(Test_MemmyCliInvalidOptionsAndNameNotFound),
    TestCase_Make(Test_MemmyCliExitCodeMapping), );
