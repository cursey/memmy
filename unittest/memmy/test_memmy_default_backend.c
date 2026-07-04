#include "test_memmy_common.h"

Test(Test_MemmyDefaultBackendReadWriteCallbacks)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS || OS_MACOS
    AssertEq(status, Memmy_Status_Ok);
    AssertTrue(ctx.backend != 0);
    AssertTrue(ctx.backend->read != 0);
    AssertTrue(ctx.backend->write != 0);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

Test(Test_MemmyDefaultBackendReadWriteSelfProcess)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS || OS_MACOS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    volatile U32 value = 0x11223344;
    U32 read_value = 0;
    U32 write_value = 0x55667788;
    U64 byte_count = 0;
    Memmy_Process *process = 0;

    AssertEq(Memmy_Process_Open(arena, Os_GetProcessId(), &process, &error), Memmy_Status_Ok);
    AssertEq(Memmy_Process_Read(process, (Memmy_Addr)(uintptr_t)&value, &read_value, sizeof(read_value), &byte_count,
                                &error),
             Memmy_Status_Ok);
    AssertEq(byte_count, sizeof(read_value));
    AssertEq(read_value, 0x11223344);

    AssertEq(Memmy_Process_Write(process, (Memmy_Addr)(uintptr_t)&value, &write_value, sizeof(write_value), &byte_count,
                                 &error),
             Memmy_Status_Ok);
    AssertEq(byte_count, sizeof(write_value));
    AssertEq(value, 0x55667788);

    Memmy_Process_Close(process);
    Memmy_Context_Pop(old_ctx);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

Test(Test_MemmyDefaultBackendSelfProcessInventoryAndScan)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS || OS_MACOS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    U8 fixture[32] = {0};
    U8 pattern_bytes[] = {0xde, 0xad, 0xbe, 0xef};
    memcpy(fixture + 8, pattern_bytes, sizeof(pattern_bytes));

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, Os_GetProcessId(), &process, &error), Memmy_Status_Ok);
    AssertTrue(process->pointer_width == Memmy_PointerWidth_32 || process->pointer_width == Memmy_PointerWidth_64);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, &error), Memmy_Status_Ok);
    AssertTrue(modules.list.count > 0);
    B32 saw_module_metadata = 0;
    List_ForEach(Memmy_Module, module, &modules.list, link)
    {
        if (module->name.len != 0 && module->path.len != 0 && module->base != 0 && module->size != 0)
        {
            saw_module_metadata = 1;
            break;
        }
    }
    AssertTrue(saw_module_metadata);

#if OS_MACOS
    Memmy_Module *main_module = ContainerOf(modules.list.first, Memmy_Module, link);
    AssertTrue(main_module->size < Gigabytes(1));
#endif

    Memmy_RegionList regions = {0};
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, &error), Memmy_Status_Ok);
    AssertTrue(regions.list.count > 0);
    B32 saw_fixture_region = 0;
    Memmy_Addr fixture_addr = (Memmy_Addr)(uintptr_t)(fixture + 8);
    List_ForEach(Memmy_Region, region, &regions.list, link)
    {
        Memmy_Addr region_end = region->base + region->size;
        if (fixture_addr >= region->base && fixture_addr < region_end)
        {
            AssertEq(region->state, Memmy_RegionState_Committed);
            AssertTrue((region->access & Memmy_RegionAccess_Read) != 0);
            AssertTrue((region->access & Memmy_RegionAccess_Guard) == 0);
            saw_fixture_region = 1;
            break;
        }
    }
    AssertTrue(saw_fixture_region);

    Memmy_Pattern pattern = {
        .bytes = Arena_PushArray(arena, Memmy_PatternByte, ArrayCount(pattern_bytes)),
        .count = ArrayCount(pattern_bytes),
    };
    for (U64 i = 0; i < ArrayCount(pattern_bytes); i++)
    {
        pattern.bytes[i].value = pattern_bytes[i];
    }

    Memmy_ScanOptions options = {
        .range = {.start = (Memmy_Addr)(uintptr_t)fixture, .end = (Memmy_Addr)(uintptr_t)(fixture + sizeof(fixture))},
        .chunk_size = 3,
    };
    Memmy_ScanResultList results = {0};
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, &results, &error), Memmy_Status_Ok);
    Memmy_Addr expected[] = {fixture_addr};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Value value = {
        .type = {.kind = Memmy_TypeKind_Bytes},
        .bytes = String8_Make(pattern_bytes, ArrayCount(pattern_bytes)),
    };
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, &results, &error), Memmy_Status_Ok);
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Process_Close(process);
    Memmy_Context_Pop(old_ctx);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

Test(Test_MemmyDefaultBackendCliSelfProcessSmoke)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS || OS_MACOS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    U8 scan_fixture[16] = {0};
    U8 pattern_bytes[] = {0xde, 0xad, 0xbe, 0xef};
    memcpy(scan_fixture + 4, pattern_bytes, sizeof(pattern_bytes));

    volatile U32 poke_value = 0x11223344;
    char *pid_text = String8_ToCStr(arena, String8_PushF(arena, "%u", Os_GetProcessId()));
    U64 poke_addr = (U64)(uintptr_t)&poke_value;
    U64 scan_start = (U64)(uintptr_t)scan_fixture;

    String8 out = {0};
    char *peek_expr = String8_ToCStr(arena, String8_PushF(arena, "0x%llx : u32", poke_addr));
    char *poke_expr = String8_ToCStr(arena, String8_PushF(arena, "0x%llx : u32 = 0x55667788", poke_addr));
    char *pscan_expr = String8_ToCStr(arena, String8_PushF(arena, "0x%llx:+0x10{de ad be ef}", scan_start));
    char *scan_expr = String8_ToCStr(arena, String8_PushF(arena, "0x%llx:+0x10 : bytes == de ad be ef", scan_start));
    char *peek_argv[] = {"memmy", "--pid", pid_text, "--expr", peek_expr};
    char *poke_argv[] = {"memmy", "--pid", pid_text, "--expr", poke_expr};
    char *pscan_argv[] = {"memmy", "--pid", pid_text, "--expr", pscan_expr};
    char *scan_argv[] = {"memmy", "--pid", pid_text, "--expr", scan_expr};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(poke_argv), poke_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(poke_value, 0x55667788);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(pscan_argv), pscan_argv, &out, &error), Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(scan_argv), scan_argv, &out, &error), Memmy_Status_Ok);

    Memmy_Context_Pop(old_ctx);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}
TestSuite suite_memmy_default_backend =
    TestSuite_Make("Memmy Default Backend", TestCase_Make(Test_MemmyDefaultBackendReadWriteCallbacks),
                   TestCase_Make(Test_MemmyDefaultBackendReadWriteSelfProcess),
                   TestCase_Make(Test_MemmyDefaultBackendSelfProcessInventoryAndScan),
                   TestCase_Make(Test_MemmyDefaultBackendCliSelfProcessSmoke), );
