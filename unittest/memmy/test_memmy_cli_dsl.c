#include "test_memmy_common.h"

static void Test_MemmyCliExpr_SetupBackend(Test_MemmyBackend *backend)
{
    Test_MemmyBackend_Init(backend);
    Test_MemmyBackend_AddProcess(backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\game\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(backend, 5678, String8_Lit("other"), String8_Lit("C:\\other\\other.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(backend, 6789, String8_Lit("other.exe"), String8_Lit("C:\\other\\other.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\game\\client.dll"), 0x1000,
                                0x2000);
    Test_MemmyBackend_AddModule(backend, 5678, String8_Lit("client.dll"), String8_Lit("C:\\other\\client.dll"), 0x3000,
                                0x2000);
    Test_MemmyBackend_AddModule(backend, 6789, String8_Lit("module"), String8_Lit("C:\\other\\module"), 0x5000, 0x2000);
    Test_MemmyBackend_AddRegion(backend, 1234, 0x1000, TEST_MEMMY_BACKEND_MEMORY_SIZE,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
}

static void Test_MemmyCliExpr_WriteU64LE(Test_MemmyBackend *backend, Memmy_Addr addr, U64 value)
{
    U64 offset = addr - backend->memory_base;
    for (U64 i = 0; i < 8; i++)
    {
        backend->memory[offset + i] = (U8)(value >> (i * 8));
    }
}

typedef struct Test_MemmyCliExprWriter Test_MemmyCliExprWriter;
struct Test_MemmyCliExprWriter
{
    Arena *arena;
    String8List chunks;
    U64 call_count;
    U64 fail_call;
};

static Memmy_Status Test_MemmyCliExprWriter_Write(void *user_data, String8 text)
{
    Test_MemmyCliExprWriter *writer = (Test_MemmyCliExprWriter *)user_data;
    writer->call_count++;
    if (writer->fail_call != 0 && writer->call_count == writer->fail_call)
    {
        return Memmy_Status_AccessDenied;
    }
    String8List_Push(writer->arena, &writer->chunks, text);
    return Memmy_Status_Ok;
}

Test(Test_MemmyCliExprResolvesModuleAddressByPid)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "<client.dll>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001123\n"));
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprResolvesPointerChainByPid)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x1123);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1123, 0x2000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "<client.dll>+0x123->0x8"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000002008\n"));
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 1234);
    AssertEq(test_backend.read_call_count, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprResolvesQualifiedProcessName)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "<game.exe!client.dll>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001123\n"));
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsExternalPidConflict)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "<other!client.dll>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertEq(test_backend.open_call_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsExternalNameConflict)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--name", "game.exe", "--expr", "<other.exe!module>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertEq(test_backend.open_call_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsJsonlAddress)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "<client.dll>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"address\",\"address\":\"0x0000000000001123\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprProcsFormatsText)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.processes[1].pointer_width = Memmy_PointerWidth_32;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "procs"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n"
                                 "4242    x64   test-process\n"
                                 "1234    x86   game.exe\n"
                                 "5678    x64   other\n"
                                 "6789    x64   other.exe\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprProcsFormatsJsonl)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "procs"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("{\"type\":\"process\",\"pid\":4242,\"arch\":\"x64\","), 0) !=
               STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("{\"type\":\"process\",\"pid\":1234,\"arch\":\"x64\","), 0) !=
               STRING8_NPOS);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprProcsFiltersFuzzyNoCase)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyBackend_AddProcess(&test_backend, 7777, String8_Lit("CoolClient.exe"),
                                 String8_Lit("C:\\game\\CoolClient.exe"), Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "procs CLIENT"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n"
                                 "7777    x64   CoolClient.exe\n"));

    error = (Memmy_Error){0};
    char *fuzzy_argv[] = {"memmy", "--expr", "procs CClt"};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(fuzzy_argv), fuzzy_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n"
                                 "7777    x64   CoolClient.exe\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRunsStatementSyntax)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "procs"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("PID     ARCH   NAME\n"), 0) != STRING8_NPOS);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliScriptMixesStatementsAssignmentsExpressionsAndExit)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy"};

    AssertEq(Memmy_Cli_RunInputString(arena, (I32)ArrayCount(argv), argv,
                                      String8_Lit("procs\n"
                                                  "$addr = <game.exe!client.dll>+0x20\n"
                                                  "$addr : u8\n"
                                                  "exit\n"
                                                  "0x1000\n"),
                                      &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n"
                                 "4242    x64   test-process\n"
                                 "1234    x64   game.exe\n"
                                 "5678    x64   other\n"
                                 "6789    x64   other.exe\n"
                                 "0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliVarsFormatsTextAndJsonl)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *text_argv[] = {"memmy"};
    char *jsonl_argv[] = {"memmy", "--jsonl"};

    AssertEq(Memmy_Cli_RunInputString(arena, (I32)ArrayCount(text_argv), text_argv,
                                      String8_Lit("$addr = <game.exe!client.dll>\n"
                                                  "vars\n"),
                                      &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("addr address\n"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunInputString(arena, (I32)ArrayCount(jsonl_argv), jsonl_argv,
                                      String8_Lit("$addr = <game.exe!client.dll>\n"
                                                  "vars\n"),
                                      &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"assignment\",\"name\":\"addr\",\"kind\":\"address\"}\n"
                                 "{\"type\":\"variable\",\"name\":\"addr\",\"kind\":\"address\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsScanTweakables)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *limit_argv[] = {"memmy", "--pid", "1234", "--limit", "1", "--expr", "0x1000:+0x10 : u8 == 42"};
    char *chunk_size_argv[] = {"memmy", "--pid", "1234", "--chunk-size", "16", "--expr", "0x1000:+0x10 : u8 == 42"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(limit_argv), limit_argv, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--limit"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(chunk_size_argv), chunk_size_argv, &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--chunk-size"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprParseErrorJsonlHasTypedFields)
{
    Arena *arena = Arena_CreateDefault();

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "0x"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));
    AssertStrEq(error.input, String8_Lit("0x"));
    AssertEq(error.byte_offset, 2);
    AssertEq(error.byte_count, 1);
    AssertStrEq(error.message, String8_Lit("expected hexadecimal digit"));
    AssertStrEq(Memmy_Cli_FormatJsonlError(arena, &error),
                String8_Lit("{\"type\":\"error\",\"status\":\"parse_error\",\"message\":\"expected "
                            "hexadecimal digit\",\"context\":\"expr\",\"input\":\"0x\",\"byte_offset\":2,"
                            "\"byte_count\":1,\"os_code\":0}\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsBareWholeProcessTargetOutsideScans)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *address_argv[] = {"memmy", "--expr", "<game.exe!>"};
    char *peek_argv[] = {"memmy", "--expr", "<game.exe!> : u8"};
    char *poke_argv[] = {"memmy", "--expr", "<game.exe!> : u8 = 42"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(address_argv), address_argv, &out, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));
    AssertStrEq(error.message, String8_Lit("whole-process target is not a valid address base"));
    AssertEq(error.byte_offset, 0);
    AssertEq(error.byte_count, 11);

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));
    AssertStrEq(error.message, String8_Lit("whole-process target is not a valid address base"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));
    AssertStrEq(error.message, String8_Lit("whole-process target is not a valid address base"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPeekTextLikePeekCommand)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x1123);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1123, 1337);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "<game.exe!client.dll>+0x123 : u32"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001123: u32 1337  0x00000539\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPeekJsonlLikePeekCommand)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1010, 42);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "0x1010 : u32"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"peek\",\"address\":\"0x0000000000001010\",\"value_type\":\"u32\","
                                 "\"value\":42,\"hex\":\"0x0000002a\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsVariableWidthStringPeek)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x1123);
    U8 str[] = {'h', 'e', 'l', 'l', 'o', 0, 'x'};
    U8 wstr[] = {'H', 0, 'i', 0, 0, 0, 'x', 0};
    for (U64 i = 0; i < sizeof(str); i++)
    {
        test_backend.memory[i] = str[i];
    }
    for (U64 i = 0; i < sizeof(wstr); i++)
    {
        test_backend.memory[0x10 + i] = wstr[i];
    }

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *str_argv[] = {"memmy", "--expr", "<game.exe!client.dll>+0x123 : str"};
    char *wstr_argv[] = {"memmy", "--expr", "<game.exe!client.dll>+0x133 : wstr"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001123: str \"hello\"\n"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001133: wstr \"Hi\"\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPokeTextLikePokeCommand)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1020, 100);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "0x1020 : u32 = 1337"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("wrote:\n"
                                 "  process: 1234\n"
                                 "  address: 0x0000000000001020\n"
                                 "  type:    u32\n"
                                 "  old:     100  0x00000064\n"
                                 "  new:     1337  0x00000539\n"));
    AssertEq(test_backend.memory[0x20], 0x39);
    AssertEq(test_backend.memory[0x21], 0x05);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPokeJsonlLikePokeCommand)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x1123);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1123, 9);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "<game.exe!client.dll>+0x123 : i32 = 77"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"poke\",\"process\":1234,\"address\":\"0x0000000000001123\","
                                 "\"value_type\":\"i32\",\"old\":\"9  0x00000009\",\"new\":\"77  0x0000004d\","
                                 "\"dry_run\":false}\n"));
    AssertEq(test_backend.memory[0], 0x4d);
    AssertEq(test_backend.memory[1], 0x00);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPatternScanTextLikePscan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x10] = 0x48;
    test_backend.memory[0x11] = 0x8b;
    test_backend.memory[0x12] = 0xaa;
    test_backend.memory[0x13] = 0xbb;
    test_backend.memory[0x14] = 0x89;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "<client.dll>[0x0:+0x40]{48 8b ?? ?? 89}"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n"
                                 "0x0000000000001010\n"));
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPatternScanJsonlLikePscan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x20] = 0x90;
    test_backend.memory[0x30] = 0x90;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "<game.exe!client.dll>[0x20:+0x20]{90}"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"match\",\"address\":\"0x0000000000001020\"}\n"
                                 "{\"type\":\"match\",\"address\":\"0x0000000000001030\"}\n"
                                 "{\"type\":\"summary\",\"matches\":2}\n"));
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsValueScanTextLikeScan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x10] = 0x90;
    test_backend.memory[0x30] = 0x90;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "<game.exe!client.dll>[0x10:+0x30] : u8 == 144"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n"
                                 "0x0000000000001010\n"
                                 "0x0000000000001030\n"));
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsValueScanJsonlLikeScan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x22] = 42;
    test_backend.memory[0x2a] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "0x1020:+0x10 : u8 == 42"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"match\",\"address\":\"0x0000000000001022\"}\n"
                                 "{\"type\":\"match\",\"address\":\"0x000000000000102a\"}\n"
                                 "{\"type\":\"summary\",\"matches\":2}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprJsonlScanWriterFailureStopsBeforeSummary)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.memory[0x22] = 42;
    test_backend.memory[0x2a] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "0x1020:+0x10 : u8 == 42"};
    Test_MemmyCliExprWriter writer_state = {
        .arena = arena,
        .fail_call = 2,
    };
    Memmy_CliOutputWriter writer = {
        .write = Test_MemmyCliExprWriter_Write,
        .user_data = &writer_state,
    };

    AssertEq(Memmy_Cli_RunToWriter(arena, (I32)ArrayCount(argv), argv, writer, &error), Memmy_Status_AccessDenied);
    AssertEq(writer_state.call_count, 2);
    String8 out = String8List_Join(arena, &writer_state.chunks, (String8){0});
    AssertStrEq(out, String8_Lit("{\"type\":\"match\",\"address\":\"0x0000000000001022\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprScansWholeProcessValueWithRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 1234, 0x1010, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 1234, 0x1040, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 1234, 0x1080, 0x10, Memmy_RegionAccess_Write,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x12] = 42;
    test_backend.memory[0x45] = 42;
    test_backend.memory[0x85] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--expr", "<game.exe!> : u8 == 42"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("ADDRESS\n"
                                 "0x0000000000001012\n"
                                 "0x0000000000001045\n"));
    AssertEq(test_backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_dsl = TestSuite_Make(
    "Memmy CLI DSL", TestCase_Make(Test_MemmyCliExprResolvesModuleAddressByPid),
    TestCase_Make(Test_MemmyCliExprResolvesPointerChainByPid),
    TestCase_Make(Test_MemmyCliExprResolvesQualifiedProcessName),
    TestCase_Make(Test_MemmyCliExprRejectsExternalPidConflict),
    TestCase_Make(Test_MemmyCliExprRejectsExternalNameConflict), TestCase_Make(Test_MemmyCliExprFormatsJsonlAddress),
    TestCase_Make(Test_MemmyCliExprProcsFormatsText), TestCase_Make(Test_MemmyCliExprProcsFormatsJsonl),
    TestCase_Make(Test_MemmyCliExprProcsFiltersFuzzyNoCase), TestCase_Make(Test_MemmyCliExprRunsStatementSyntax),
    TestCase_Make(Test_MemmyCliScriptMixesStatementsAssignmentsExpressionsAndExit),
    TestCase_Make(Test_MemmyCliVarsFormatsTextAndJsonl), TestCase_Make(Test_MemmyCliExprRejectsScanTweakables),
    TestCase_Make(Test_MemmyCliExprParseErrorJsonlHasTypedFields),
    TestCase_Make(Test_MemmyCliExprRejectsBareWholeProcessTargetOutsideScans),
    TestCase_Make(Test_MemmyCliExprFormatsPeekTextLikePeekCommand),
    TestCase_Make(Test_MemmyCliExprFormatsPeekJsonlLikePeekCommand),
    TestCase_Make(Test_MemmyCliExprFormatsVariableWidthStringPeek),
    TestCase_Make(Test_MemmyCliExprFormatsPokeTextLikePokeCommand),
    TestCase_Make(Test_MemmyCliExprFormatsPokeJsonlLikePokeCommand),
    TestCase_Make(Test_MemmyCliExprFormatsPatternScanTextLikePscan),
    TestCase_Make(Test_MemmyCliExprFormatsPatternScanJsonlLikePscan),
    TestCase_Make(Test_MemmyCliExprFormatsValueScanTextLikeScan),
    TestCase_Make(Test_MemmyCliExprFormatsValueScanJsonlLikeScan),
    TestCase_Make(Test_MemmyCliExprJsonlScanWriterFailureStopsBeforeSummary),
    TestCase_Make(Test_MemmyCliExprScansWholeProcessValueWithRegions), );
