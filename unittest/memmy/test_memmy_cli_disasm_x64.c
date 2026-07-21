#include "test_memmy_common.h"

static void Test_CliDisasm_SetupBackend(Test_MemmyBackend *backend)
{
    Test_MemmyBackend_Init(backend);
    Test_MemmyBackend_AddProcess(backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\game\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\game\\client.dll"), 0x1000,
                                0x2000);
    Test_MemmyBackend_AddRegion(backend, 1234, 0x1000, TEST_MEMMY_BACKEND_MEMORY_SIZE,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
}

static void Test_CliDisasm_WriteBytes(Test_MemmyBackend *backend, Memmy_Addr address, U8 *bytes, U64 count)
{
    U64 offset = address - backend->memory_base;
    for (U64 i = 0; i < count; i++)
    {
        backend->memory[offset + i] = bytes[i];
    }
}

Test(Test_MemmyCliDisasmX64FormatsModuleRangeScan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_CliDisasm_SetupBackend(&backend);

    U8 code[] = {0x8b, 0x05, 0x78, 0x56, 0x34, 0x12, 0x48, 0x31, 0xc0};
    Test_CliDisasm_WriteBytes(&backend, 0x1010, code, ArrayCount(code));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr",
                    "<client.dll> disasm x64 { mov reg, [rip+disp32]; xor rax, rax }"};

    AssertEq(MemmyCli_Argv_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[0] address 0x0000000000001010\n"
                                 "list<address> count 1\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliDisasmX64FormatsWholeProcessScan)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_CliDisasm_SetupBackend(&backend);
    backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&backend, 1234, 0x1010, 0x20, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 1234, 0x1040, 0x20, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    U8 code[] = {0x8b, 0x05, 0x78, 0x56, 0x34, 0x12, 0x48, 0x31, 0xc0};
    Test_CliDisasm_WriteBytes(&backend, 0x1010, code, ArrayCount(code));
    Test_CliDisasm_WriteBytes(&backend, 0x1040, code, ArrayCount(code));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--name", "game.exe", "--expr",
                    "[0..] disasm x64 { mov reg, [rip+disp32]; xor rax, rax }"};

    AssertEq(MemmyCli_Argv_RunToString(arena, (I32)ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[0] address 0x0000000000001010\n"
                                 "[1] address 0x0000000000001040\n"
                                 "list<address> count 2\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_disasm_x64 =
    TestSuite_Make("Memmy CLI Disasm X64", TestCase_Make(Test_MemmyCliDisasmX64FormatsModuleRangeScan),
                   TestCase_Make(Test_MemmyCliDisasmX64FormatsWholeProcessScan), );
