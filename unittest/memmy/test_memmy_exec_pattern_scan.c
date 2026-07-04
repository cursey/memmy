#include "test_memmy_common.h"

#include "memmy_exec.h"

static void Test_MemmyExecPatternScan_Parse(Arena *arena, char *text, Memmy_MemoryExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

Test(Test_MemmyExecPatternScanExecutesWildcardPattern)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x1000,
                                0x100);
    backend.memory[0x20] = 0x48;
    backend.memory[0x21] = 0x8b;
    backend.memory[0x22] = 0xaa;
    backend.memory[0x23] = 0xbb;
    backend.memory[0x24] = 0x89;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query, &process, &error),
             Memmy_Status_Ok);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, &error), Memmy_Status_Ok);

    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPatternScan_Parse(arena, "<client.dll>{48 8b ?? ?? 89}", &expr);

    Memmy_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePatternScan(arena, process, &modules, &expr, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1020};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecPatternScanUsesDefaultOptions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x1000,
                                0x80);
    backend.memory[0x10] = 0x90;
    backend.memory[0x30] = 0x90;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query, &process, &error),
             Memmy_Status_Ok);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, &error), Memmy_Status_Ok);

    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPatternScan_Parse(arena, "<client.dll>[0x10:+0x30]{90}", &expr);

    Memmy_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePatternScan(arena, process, &modules, &expr, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1010, 0x1030};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));
    AssertEq(backend.min_read_addr, 0x1010);
    AssertEq(backend.max_read_end, 0x1040);

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_pattern_scan =
    TestSuite_Make("Memmy Exec Pattern Scan", TestCase_Make(Test_MemmyExecPatternScanExecutesWildcardPattern),
                   TestCase_Make(Test_MemmyExecPatternScanUsesDefaultOptions), );
