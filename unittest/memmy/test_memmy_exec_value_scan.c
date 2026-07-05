#include "test_memmy_common.h"

#include "memmy_exec.h"

#include <string.h>

static void Test_MemmyExecValueScan_Parse(Arena *arena, char *text, Memmy_MemoryExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_MemmyExecValueScan_WriteU32LE(Test_MemmyBackend *backend, Memmy_Addr addr, U32 value)
{
    U64 offset = addr - backend->memory_base;
    for (U64 i = 0; i < 4; i++)
    {
        backend->memory[offset + i] = (U8)(value >> (i * 8));
    }
}

static void Test_MemmyExecValueScan_Open(Arena *arena, Test_MemmyBackend *backend, Memmy_Process **out)
{
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);
    *out = process;
}

Test(Test_MemmyExecValueScanWholeProcessRequiresRegionEnumeration)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_DisableEnumerateRegions(&backend);

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "<4242!> : u8 == 42", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanRejectsWholeProcessAddressPeekPoke)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_Error error = {0};
    Memmy_MemoryExpr expr = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("<game.exe!>"), &expr, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("<game.exe!> : i32"), &expr, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));

    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("<game.exe!> : i32 = 42"), &expr, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanRejectsOrderingComparisons)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_Error error = {0};
    Memmy_MemoryExpr expr = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("<game.exe!> : i32 > 42"), &expr, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("expr"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanScansModuleRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x1000,
                                0x80);
    Test_MemmyExecValueScan_WriteU32LE(&backend, 0x1010, 42);
    Test_MemmyExecValueScan_WriteU32LE(&backend, 0x1030, 42);
    Test_MemmyExecValueScan_WriteU32LE(&backend, 0x1090, 42);

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "<client.dll> : u32 == 42", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1010, 0x1030};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanScansAddressSizedRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.memory[0x25] = 171;
    backend.memory[0x35] = 171;

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "0x1020:+0x10 : u8 == 171", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1025};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanScansWholeProcessRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1010, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1040, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1080, 0x10, Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
    backend.memory[0x12] = 42;
    backend.memory[0x45] = 42;
    backend.memory[0x85] = 42;

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "<4242!> : u8 == 42", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1012, 0x1045};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanFindsValueAcrossAdjacentWholeProcessRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    U8 bytes[] = {0xca, 0xfe, 0xba, 0xbe};
    memcpy(backend.memory + 0x2e, bytes, sizeof(bytes));
    memcpy(backend.memory + 0x4e, bytes, sizeof(bytes));

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "<4242!> : bytes == ca fe ba be", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x102e};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecValueScanUsesDefaultOptions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x1000,
                                0x40);
    backend.memory[0x10] = 0x90;
    backend.memory[0x30] = 0x90;

    Memmy_Process *process = 0;
    Test_MemmyExecValueScan_Open(arena, &backend, &process);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecValueScan_Parse(arena, "<client.dll>[0x10:+0x30] : u8 == 144", &expr);

    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    AssertEq(Memmy_MemoryExpr_ExecuteValueScan(arena, process, &expr, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1010, 0x1030};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));
    AssertEq(backend.min_read_addr, 0x1010);
    AssertEq(backend.max_read_end, 0x1040);

    Memmy_Process_Close(process);
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_value_scan =
    TestSuite_Make("Memmy Exec Value Scan",
                   TestCase_Make(Test_MemmyExecValueScanWholeProcessRequiresRegionEnumeration),
                   TestCase_Make(Test_MemmyExecValueScanRejectsWholeProcessAddressPeekPoke),
                   TestCase_Make(Test_MemmyExecValueScanRejectsOrderingComparisons),
                   TestCase_Make(Test_MemmyExecValueScanScansModuleRange),
                   TestCase_Make(Test_MemmyExecValueScanScansAddressSizedRange),
                   TestCase_Make(Test_MemmyExecValueScanScansWholeProcessRegions),
                   TestCase_Make(Test_MemmyExecValueScanFindsValueAcrossAdjacentWholeProcessRegions),
                   TestCase_Make(Test_MemmyExecValueScanUsesDefaultOptions), );
