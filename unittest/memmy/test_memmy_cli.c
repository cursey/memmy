#include "test_memmy_common.h"

#include "base_fs.h"

static void Test_MemmyCli_SetupInputBackend(Test_MemmyBackend *backend)
{
    Test_MemmyBackend_Init(backend);
    Test_MemmyBackend_AddProcess(backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\game\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\game\\client.dll"), 0x1000,
                                0x2000);
}

Test(Test_MemmyCliHelpAndVersion)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *help_argv[] = {"memmy", "--help"};
    char *version_argv[] = {"memmy", "--version"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(help_argv), help_argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("memmy [global-options] [--pid <pid>|--name <name>] <file>"), 0) !=
               STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("--expr <memory-expr>"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("procs"), 0) == STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(version_argv), version_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("memmy 0.0.0\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliRejectsAmbiguousInputSources)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *expr_file[] = {"memmy", "--expr", "0x1000", "script.memmy"};
    char *two_files[] = {"memmy", "a.memmy", "b.memmy"};
    char *stdin_expr[] = {"memmy", "--expr", "0x1000"};
    char *stdin_file[] = {"memmy", "script.memmy"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(expr_file), expr_file, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--expr"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(two_files), two_files, &out, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("b.memmy"));

    error = (Memmy_Error){0};
    AssertEq(
        Memmy_Cli_RunInputString(arena, (I32)ArrayCount(stdin_expr), stdin_expr, String8_Lit("0x1000\n"), &out, &error),
        Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--expr"));

    error = (Memmy_Error){0};
    AssertEq(
        Memmy_Cli_RunInputString(arena, (I32)ArrayCount(stdin_file), stdin_file, String8_Lit("0x1000\n"), &out, &error),
        Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("script.memmy"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliInputStringEvaluatesWithOptions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCli_SetupInputBackend(&test_backend);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234"};
    AssertEq(Memmy_Cli_RunInputString(arena, (I32)ArrayCount(argv), argv, String8_Lit("<client.dll>+0x20 : u8\n"), &out,
                                      &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliFileInputEvaluatesReplString)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCli_SetupInputBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 path = Fs_TempFile(arena, String8_Lit("memmy-test.memmy"));
    AssertTrue(Fs_WriteFile(path, String8_Lit("<game.exe!client.dll>\nexit\n0x1000\n")));

    Scratch scratch = Scratch_Begin(&arena, 1);
    char *path_text = String8_ToCStr(scratch.arena, path);
    char *argv[] = {"memmy", path_text};
    String8 out = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001000\n"));
    Scratch_End(scratch);

    AssertTrue(Os_FileDelete(path));
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliFormerSubcommandNamesAreFilePaths)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "peek"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("file"));
    AssertStrEq(error.input, String8_Lit("peek"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliJsonHelpers)
{
    Arena *arena = Arena_CreateDefault();
    U8 bytes[] = {0x00, 0x0a, 0xff};
    char *json_flag[] = {"memmy", "--json", "--expr", "0x1000"};
    char *jsonl_flag[] = {"memmy", "--jsonl", "--expr", "0x1000:+0x10 : u8 == 1"};
    char *json_value[] = {"memmy", "--expr", "--json"};
    char *jsonl_value[] = {"memmy", "--expr", "--jsonl"};
    char *help_flag[] = {"memmy", "--help"};
    char *version_flag[] = {"memmy", "--version"};
    char *help_value[] = {"memmy", "--expr", "--help"};
    char *version_value[] = {"memmy", "--expr", "--version"};

    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_64, 0x4242), String8_Lit("0x0000000000004242"));
    AssertStrEq(Memmy_Cli_FormatAddress(arena, Memmy_PointerWidth_32, 0x4242), String8_Lit("0x00004242"));
    AssertStrEq(Memmy_Cli_FormatHexBytes(arena, String8_Make(bytes, ArrayCount(bytes))), String8_Lit("00 0a ff"));
    AssertStrEq(Memmy_Cli_FormatJsonString(arena, String8_Lit("a\0b\n\"\\")), String8_Lit("\"a\\u0000b\\n\\\"\\\\\""));
    AssertEq(Memmy_Cli_ArgvHasJson((I32)ArrayCount(json_flag), json_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasJsonl((I32)ArrayCount(jsonl_flag), jsonl_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasJson((I32)ArrayCount(json_value), json_value), 0);
    AssertEq(Memmy_Cli_ArgvHasJsonl((I32)ArrayCount(jsonl_value), jsonl_value), 0);
    AssertEq(Memmy_Cli_ArgvHasHelp((I32)ArrayCount(help_flag), help_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasVersion((I32)ArrayCount(version_flag), version_flag), 1);
    AssertEq(Memmy_Cli_ArgvHasHelp((I32)ArrayCount(help_value), help_value), 0);
    AssertEq(Memmy_Cli_ArgvHasVersion((I32)ArrayCount(version_value), version_value), 0);

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

Test(Test_MemmyCliInvalidOptions)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *duplicate_json[] = {"memmy", "--json", "--json", "--expr", "0x1000"};
    char *invalid_script_option[] = {"memmy", "--addr", "0x1000", "script.memmy"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(duplicate_json), duplicate_json, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--json"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(invalid_script_option), invalid_script_option, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.message, String8_Lit("option is invalid for --expr"));

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
    "Memmy CLI", TestCase_Make(Test_MemmyCliHelpAndVersion), TestCase_Make(Test_MemmyCliRejectsAmbiguousInputSources),
    TestCase_Make(Test_MemmyCliInputStringEvaluatesWithOptions),
    TestCase_Make(Test_MemmyCliFileInputEvaluatesReplString),
    TestCase_Make(Test_MemmyCliFormerSubcommandNamesAreFilePaths), TestCase_Make(Test_MemmyCliJsonHelpers),
    TestCase_Make(Test_MemmyCliInvalidOptions), TestCase_Make(Test_MemmyCliExitCodeMapping), );
