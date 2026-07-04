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

TestSuite suite_memmy_cli_expr = TestSuite_Make(
    "Memmy CLI Expr", TestCase_Make(Test_MemmyCliExprResolvesModuleAddressByPid),
    TestCase_Make(Test_MemmyCliExprResolvesPointerChainByPid),
    TestCase_Make(Test_MemmyCliExprResolvesQualifiedProcessName),
    TestCase_Make(Test_MemmyCliExprRejectsExternalPidConflict),
    TestCase_Make(Test_MemmyCliExprRejectsExternalNameConflict), TestCase_Make(Test_MemmyCliExprFormatsJsonAddress), );
