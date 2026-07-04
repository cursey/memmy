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
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Query);

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
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);
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

Test(Test_MemmyCliExprFormatsJsonAddress)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--json", "--pid", "1234", "--expr", "<client.dll>+0x123"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001123\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsJsonlAddress)
{
    Arena *arena = Arena_CreateDefault();

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "0x1000"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--jsonl"));

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

Test(Test_MemmyCliExprFormatsPeekJsonLikePeekCommand)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyCliExpr_SetupBackend(&test_backend);
    Test_MemmyCliExpr_WriteU64LE(&test_backend, 0x1010, 42);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--json", "--pid", "1234", "--expr", "0x1010 : u32"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001010\",\"type\":\"u32\",\"value\":42,"
                                 "\"hex\":\"0x0000002a\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprRejectsJsonlPeek)
{
    Arena *arena = Arena_CreateDefault();

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "0x1000 : u32"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--jsonl"));

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

Test(Test_MemmyCliExprRejectsJsonlPoke)
{
    Arena *arena = Arena_CreateDefault();

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl", "--expr", "0x1000 : u32 = 1"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.input, String8_Lit("--jsonl"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliExprFormatsPokeJsonLikePokeCommand)
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
    char *argv[] = {"memmy", "--json", "--expr", "<game.exe!client.dll>+0x123 : i32 = 77"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"process\":1234,\"address\":\"0x0000000000001123\",\"type\":\"i32\","
                                 "\"old\":\"9  0x00000009\",\"new\":\"77  0x0000004d\",\"dry_run\":false}\n"));
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
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);

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
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\"}\n"
                                 "{\"address\":\"0x0000000000001030\"}\n"));
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
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);

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
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001022\"}\n"
                                 "{\"address\":\"0x000000000000102a\"}\n"));

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
    AssertEq(test_backend.last_open_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_expr = TestSuite_Make(
    "Memmy CLI Expr", TestCase_Make(Test_MemmyCliExprResolvesModuleAddressByPid),
    TestCase_Make(Test_MemmyCliExprResolvesPointerChainByPid),
    TestCase_Make(Test_MemmyCliExprResolvesQualifiedProcessName),
    TestCase_Make(Test_MemmyCliExprRejectsExternalPidConflict),
    TestCase_Make(Test_MemmyCliExprRejectsExternalNameConflict), TestCase_Make(Test_MemmyCliExprFormatsJsonAddress),
    TestCase_Make(Test_MemmyCliExprRejectsJsonlAddress), TestCase_Make(Test_MemmyCliExprFormatsPeekTextLikePeekCommand),
    TestCase_Make(Test_MemmyCliExprFormatsPeekJsonLikePeekCommand), TestCase_Make(Test_MemmyCliExprRejectsJsonlPeek),
    TestCase_Make(Test_MemmyCliExprFormatsPokeTextLikePokeCommand), TestCase_Make(Test_MemmyCliExprRejectsJsonlPoke),
    TestCase_Make(Test_MemmyCliExprFormatsPokeJsonLikePokeCommand),
    TestCase_Make(Test_MemmyCliExprFormatsPatternScanTextLikePscan),
    TestCase_Make(Test_MemmyCliExprFormatsPatternScanJsonlLikePscan),
    TestCase_Make(Test_MemmyCliExprFormatsValueScanTextLikeScan),
    TestCase_Make(Test_MemmyCliExprFormatsValueScanJsonlLikeScan),
    TestCase_Make(Test_MemmyCliExprScansWholeProcessValueWithRegions), );
