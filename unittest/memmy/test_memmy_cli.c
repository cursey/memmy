#include "test_memmy_common.h"

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
    char *poke_expr_option[] = {"memmy",  "poke", "--pid",   "4242", "--addr", "0x1000",
                                "--type", "u8",   "--value", "1",    "--expr", "0x1000"};
    char *scan_expr_option[] = {"memmy", "scan",   "--pid", "4242",    "--start", "0x1000", "--length",
                                "0x10",  "--type", "u8",    "--value", "1",       "--expr", "0x1000"};
    char *pscan_expr_option[] = {"memmy",    "pscan", "--pid",     "4242", "--start", "0x1000",
                                 "--length", "0x10",  "--pattern", "90",   "--expr",  "0x1000"};
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
        {poke_expr_option, (I32)ArrayCount(poke_expr_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {scan_expr_option, (I32)ArrayCount(scan_expr_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
        {pscan_expr_option, (I32)ArrayCount(pscan_expr_option), Memmy_Status_InvalidArgument, String8_Lit("cli")},
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
    AssertTrue(String8_Find(out, String8_Lit("--expr <memory-expr>"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("--limit and --chunk-size are only valid for scan and pscan"), 0) !=
               STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(version_argv), version_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("memmy 0.0.0\n"));

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

Test(Test_MemmyCliCommandsOpenSelectedProcess)
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
    AssertEq(test_backend.last_open_pid, 4242);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 4242);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 4242);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 4242);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 4242);

    Test_ResetOpenTracking(&test_backend);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 4242);

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
    "Memmy CLI", TestCase_Make(Test_MemmyCliRejectsPokeOptionsOnOtherCommands),
    TestCase_Make(Test_MemmyCliRejectsV0NonGoalSyntax), TestCase_Make(Test_MemmyCliHelpAndVersion),
    TestCase_Make(Test_MemmyCliJsonHelpers), TestCase_Make(Test_MemmyCliCommandsOpenSelectedProcess),
    TestCase_Make(Test_MemmyCliInvalidOptionsAndNameNotFound), TestCase_Make(Test_MemmyCliExitCodeMapping), );
