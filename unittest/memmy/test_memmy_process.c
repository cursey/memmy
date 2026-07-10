#include "test_memmy_common.h"

static void Test_MemmyProcess_WritePointer(Test_MemmyBackend *backend, Memmy_Addr address, Memmy_Addr value)
{
    U64 offset = address - backend->memory_base;
    U64 raw = value;
    memcpy(backend->memory + offset, &raw, sizeof(raw));
}

Test(Test_MemmyTestBackendReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Backend *backend = Test_MemmyBackend_AsBackend(&test_backend);
    Memmy_Context ctx = {.backend = backend};
    Memmy_Context_Set(&ctx);

    Test_ProcessInfoList processes = {0};
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&processes, arena), 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 1);
    Test_ProcessInfoNode *info = ContainerOf(processes.list.first, Test_ProcessInfoNode, link);
    AssertEq(info->info.pid, 4242);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, &process, 0), Memmy_Status_Ok);
    AssertTrue(Memmy_Process_IsOpen(process));
    AssertEq(process->pointer_width, Memmy_PointerWidth_64);

    U8 buffer[4] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(process, test_backend.memory_base + 2, buffer, sizeof(buffer), &bytes_read, 0),
             Memmy_Status_Ok);
    AssertEq(bytes_read, 4);
    AssertEq(buffer[0], 2);
    AssertEq(buffer[3], 5);

    U8 replacement[2] = {99, 100};
    U64 bytes_written = 0;
    AssertEq(
        Memmy_Process_Write(process, test_backend.memory_base + 4, replacement, sizeof(replacement), &bytes_written, 0),
        Memmy_Status_Ok);
    AssertEq(bytes_written, 2);
    AssertEq(test_backend.memory[4], 99);
    AssertEq(test_backend.memory[5], 100);

    Test_ModuleList modules = {0};
    AssertEq(Memmy_Process_EnumerateModules(arena, process, Test_ModuleSink(&modules, arena), 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Test_RegionList regions = {0};
    AssertEq(Memmy_Process_EnumerateRegions(arena, process, Test_RegionSink(&regions, arena), 0), Memmy_Status_Ok);
    AssertEq(regions.list.count, 1);

    Memmy_Process_Close(process);
    AssertTrue(!Memmy_Process_IsOpen(process));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessMissingBackendCallbacksReturnUnsupported)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, &process, 0), Memmy_Status_Ok);

    Memmy_Error error = {0};
    Test_ProcessInfoList processes = {0};
    test_backend.backend.enumerate_processes = 0;
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&processes, arena), &error),
             Memmy_Status_Unsupported);

    U8 buffer[4] = {0};
    U64 byte_count = 0;
    test_backend.backend.read = 0;
    AssertEq(Memmy_Process_Read(process, test_backend.memory_base, buffer, sizeof(buffer), &byte_count, &error),
             Memmy_Status_Unsupported);

    test_backend.backend.write = 0;
    AssertEq(Memmy_Process_Write(process, test_backend.memory_base, buffer, sizeof(buffer), &byte_count, &error),
             Memmy_Status_Unsupported);

    Test_ModuleList modules = {0};
    test_backend.backend.enumerate_modules = 0;
    AssertEq(Memmy_Process_EnumerateModules(arena, process, Test_ModuleSink(&modules, arena), &error),
             Memmy_Status_Unsupported);

    Test_RegionList regions = {0};
    test_backend.backend.enumerate_regions = 0;
    AssertEq(Memmy_Process_EnumerateRegions(arena, process, Test_RegionSink(&regions, arena), &error),
             Memmy_Status_Unsupported);

    Memmy_Range function = {0};
    test_backend.backend.find_function = 0;
    AssertEq(Memmy_Process_FindFunction(arena, process, test_backend.memory_base, &function, &error),
             Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));

    Memmy_ObjectBaseResult object_base = {0};
    test_backend.backend.find_object_base = 0;
    AssertEq(Memmy_Process_FindObjectBase(arena, process, test_backend.memory_base, 0, &object_base, &error),
             Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessReadDispatchAndFailureMapping)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x5000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    U8 buffer[8] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_Ok);
    AssertEq(bytes_read, 4);
    AssertEq(buffer[0], 1);
    AssertEq(buffer[3], 4);

    Test_MemmyBackend_SetReadLimit(&test_backend, 2);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_PartialRead);
    AssertEq(bytes_read, 2);

    Test_MemmyBackend_SetReadLimit(&test_backend, 0);
    AssertEq(Memmy_Process_Read(process, 0x6000, buffer, 4, &bytes_read, &error), Memmy_Status_Unreadable);
    AssertEq(bytes_read, 0);

    Test_MemmyBackend_SetReadStatus(&test_backend, Memmy_Status_AccessDenied);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Test_MemmyBackend_SetReadStatus(&test_backend, Memmy_Status_PlatformError);
    AssertEq(Memmy_Process_Read(process, 0x5001, buffer, 4, &bytes_read, &error), Memmy_Status_PlatformError);
    AssertEq(error.status, Memmy_Status_PlatformError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessWriteDispatchAndFailureMapping)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_SetMemoryBase(&test_backend, 0x5000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    U8 replacement[4] = {0xaa, 0xbb, 0xcc, 0xdd};
    U64 bytes_written = 0;
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_Ok);
    AssertEq(bytes_written, 4);
    AssertEq(test_backend.memory[1], 0xaa);
    AssertEq(test_backend.memory[4], 0xdd);

    Test_MemmyBackend_SetWriteLimit(&test_backend, 2);
    replacement[0] = 0x11;
    replacement[1] = 0x22;
    AssertEq(Memmy_Process_Write(process, 0x5008, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_PartialWrite);
    AssertEq(bytes_written, 2);
    AssertEq(test_backend.memory[8], 0x11);
    AssertEq(test_backend.memory[9], 0x22);

    Test_MemmyBackend_SetWriteLimit(&test_backend, 0);
    AssertEq(Memmy_Process_Write(process, 0x6000, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_Unwritable);
    AssertEq(bytes_written, 0);

    Test_MemmyBackend_SetWriteStatus(&test_backend, Memmy_Status_AccessDenied);
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Test_MemmyBackend_SetWriteStatus(&test_backend, Memmy_Status_PlatformError);
    AssertEq(Memmy_Process_Write(process, 0x5001, replacement, sizeof(replacement), &bytes_written, &error),
             Memmy_Status_PlatformError);
    AssertEq(error.status, Memmy_Status_PlatformError);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessEnumerationPreservesCallbackOrder)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit(""), Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit(""), Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 111, String8_Lit("a.dll"), String8_Lit(""), 0x1000, 0x100);
    Test_MemmyBackend_AddModule(&test_backend, 111, String8_Lit("b.dll"), String8_Lit(""), 0x2000, 0x100);
    Test_MemmyBackend_AddRegion(&test_backend, 111, 0x3000, 0x100, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 111, 0x4000, 0x100, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Test_ProcessInfoList processes = {0};
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&processes, arena), 0), Memmy_Status_Ok);
    Test_ProcessInfoNode *process_a = ContainerOf(processes.list.first, Test_ProcessInfoNode, link);
    Test_ProcessInfoNode *process_b = ContainerOf(processes.list.last, Test_ProcessInfoNode, link);
    AssertEq(process_a->info.pid, 111);
    AssertEq(process_b->info.pid, 222);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 111, &process, 0), Memmy_Status_Ok);

    Test_ModuleList modules = {0};
    AssertEq(Memmy_Process_EnumerateModules(arena, process, Test_ModuleSink(&modules, arena), 0), Memmy_Status_Ok);
    Test_ModuleNode *module_a = ContainerOf(modules.list.first, Test_ModuleNode, link);
    Test_ModuleNode *module_b = ContainerOf(modules.list.last, Test_ModuleNode, link);
    AssertStrEq(module_a->module.name, String8_Lit("a.dll"));
    AssertStrEq(module_b->module.name, String8_Lit("b.dll"));

    Test_RegionList regions = {0};
    AssertEq(Memmy_Process_EnumerateRegions(arena, process, Test_RegionSink(&regions, arena), 0), Memmy_Status_Ok);
    Test_RegionNode *region_a = ContainerOf(regions.list.first, Test_RegionNode, link);
    Test_RegionNode *region_b = ContainerOf(regions.list.last, Test_RegionNode, link);
    AssertEq(region_a->region.base, 0x3000);
    AssertEq(region_b->region.base, 0x4000);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessEnumerationPropagatesSinkErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("alpha.exe"), String8_Lit(""), Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit(""), Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 111, String8_Lit("a.dll"), String8_Lit(""), 0x1000, 0x100);
    Test_MemmyBackend_AddModule(&test_backend, 111, String8_Lit("b.dll"), String8_Lit(""), 0x2000, 0x100);
    Test_MemmyBackend_AddRegion(&test_backend, 111, 0x3000, 0x100, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 111, 0x4000, 0x100, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Test_ProcessInfoList processes = {0};
    Memmy_ProcessInfoSink process_sink = Test_ProcessInfoSink(&processes, arena);
    processes.status = Memmy_Status_AccessDenied;
    AssertEq(Memmy_EnumerateProcesses(arena, process_sink, 0), Memmy_Status_AccessDenied);
    AssertEq(processes.list.count, 1);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 111, &process, 0), Memmy_Status_Ok);

    Test_ModuleList modules = {0};
    Memmy_ModuleSink module_sink = Test_ModuleSink(&modules, arena);
    modules.status = Memmy_Status_AccessDenied;
    AssertEq(Memmy_Process_EnumerateModules(arena, process, module_sink, 0), Memmy_Status_AccessDenied);
    AssertEq(modules.list.count, 1);

    Test_RegionList regions = {0};
    Memmy_RegionSink region_sink = Test_RegionSink(&regions, arena);
    regions.status = Memmy_Status_AccessDenied;
    AssertEq(Memmy_Process_EnumerateRegions(arena, process, region_sink, 0), Memmy_Status_AccessDenied);
    AssertEq(regions.list.count, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyTestBackendConfiguredInventory)
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
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"), 0x400000,
                                0x3000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x500000, 0x1000, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Test_ProcessInfoList processes = {0};
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&processes, arena), 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 2);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 222, &process, 0), Memmy_Status_Ok);
    AssertEq(process->pointer_width, Memmy_PointerWidth_64);

    Test_ModuleList modules = {0};
    AssertEq(Memmy_Process_EnumerateModules(arena, process, Test_ModuleSink(&modules, arena), 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Test_RegionList regions = {0};
    AssertEq(Memmy_Process_EnumerateRegions(arena, process, Test_RegionSink(&regions, arena), 0), Memmy_Status_Ok);
    AssertEq(regions.list.count, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessFindObjectBaseSuccess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.regions[0].access |= Memmy_RegionAccess_Execute;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    Memmy_Addr object = test_backend.memory_base + 0x20;
    Memmy_Addr vtable = test_backend.memory_base + 0x80;
    Memmy_Addr code = test_backend.memory_base + 0xc0;
    Test_MemmyProcess_WritePointer(&test_backend, object, vtable);
    Test_MemmyProcess_WritePointer(&test_backend, vtable, code);
    Test_MemmyProcess_WritePointer(&test_backend, vtable + 8, code + 8);

    Memmy_ObjectBaseResult result = {0};
    Memmy_ObjectBaseOptions options = {0};
    Memmy_ObjectBaseOptions options_before = options;
    AssertEq(Memmy_Process_FindObjectBase(arena, process, object + 0x18, &options, &result, &error), Memmy_Status_Ok);
    AssertTrue(Memory_Equals(&options, &options_before, sizeof(options)));
    AssertEq(result.address, object);
    AssertEq(result.vptr_address, object);
    AssertEq(result.vtable, vtable);
    AssertEq(result.confidence, Memmy_ObjectBaseConfidence_Weak);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessFindObjectBaseStopsAtMaxBack)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.regions[0].access |= Memmy_RegionAccess_Execute;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    Memmy_Addr object = test_backend.memory_base + 0x20;
    Memmy_Addr vtable = test_backend.memory_base + 0x80;
    Memmy_Addr code = test_backend.memory_base + 0xc0;
    Test_MemmyProcess_WritePointer(&test_backend, object, vtable);
    Test_MemmyProcess_WritePointer(&test_backend, vtable, code);
    Test_MemmyProcess_WritePointer(&test_backend, vtable + 8, code + 8);

    Memmy_ObjectBaseOptions options = {.max_scan_back = 0x10};
    Memmy_ObjectBaseResult result = {0};
    AssertEq(Memmy_Process_FindObjectBase(arena, process, object + 0x18, &options, &result, &error),
             Memmy_Status_NotFound);
    AssertEq(error.status, Memmy_Status_NotFound);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessFindObjectBaseStopsAtRegionBoundary)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.regions[0].access |= Memmy_RegionAccess_Execute;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, test_backend.memory_base + 0x30, 0x50, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, test_backend.memory_base + 0xc0, 0x20, Memmy_RegionAccess_Execute,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    Memmy_Addr object_before_region = test_backend.memory_base + 0x20;
    Memmy_Addr vtable = test_backend.memory_base + 0x40;
    Memmy_Addr code = test_backend.memory_base + 0xc0;
    Test_MemmyProcess_WritePointer(&test_backend, object_before_region, vtable);
    Test_MemmyProcess_WritePointer(&test_backend, vtable, code);
    Test_MemmyProcess_WritePointer(&test_backend, vtable + 8, code + 8);

    Memmy_ObjectBaseResult result = {0};
    AssertEq(Memmy_Process_FindObjectBase(arena, process, test_backend.memory_base + 0x48, 0, &result, &error),
             Memmy_Status_NotFound);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessFindObjectBaseNoCandidate)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.regions[0].access |= Memmy_RegionAccess_Execute;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    Memmy_ObjectBaseResult result = {0};
    AssertEq(Memmy_Process_FindObjectBase(arena, process, test_backend.memory_base + 0x40, 0, &result, &error),
             Memmy_Status_NotFound);
    AssertEq(error.status, Memmy_Status_NotFound);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyProcessFindObjectBaseAmbiguous)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.regions[0].access |= Memmy_RegionAccess_Execute;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Process *process = 0;
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, &process, &error), Memmy_Status_Ok);

    Memmy_Addr object_a = test_backend.memory_base + 0x20;
    Memmy_Addr object_b = test_backend.memory_base + 0x30;
    Memmy_Addr vtable = test_backend.memory_base + 0x80;
    Memmy_Addr code = test_backend.memory_base + 0xc0;
    Test_MemmyProcess_WritePointer(&test_backend, object_a, vtable);
    Test_MemmyProcess_WritePointer(&test_backend, object_b, vtable);
    Test_MemmyProcess_WritePointer(&test_backend, vtable, code);
    Test_MemmyProcess_WritePointer(&test_backend, vtable + 8, code + 8);

    Memmy_ObjectBaseResult result = {0};
    AssertEq(Memmy_Process_FindObjectBase(arena, process, object_b + 0x18, 0, &result, &error), Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}
TestSuite suite_memmy_process = TestSuite_Make("Memmy Process", TestCase_Make(Test_MemmyTestBackendReadWrite),
                                               TestCase_Make(Test_MemmyProcessMissingBackendCallbacksReturnUnsupported),
                                               TestCase_Make(Test_MemmyProcessReadDispatchAndFailureMapping),
                                               TestCase_Make(Test_MemmyProcessWriteDispatchAndFailureMapping),
                                               TestCase_Make(Test_MemmyProcessEnumerationPreservesCallbackOrder),
                                               TestCase_Make(Test_MemmyProcessEnumerationPropagatesSinkErrors),
                                               TestCase_Make(Test_MemmyTestBackendConfiguredInventory),
                                               TestCase_Make(Test_MemmyProcessFindObjectBaseSuccess),
                                               TestCase_Make(Test_MemmyProcessFindObjectBaseStopsAtMaxBack),
                                               TestCase_Make(Test_MemmyProcessFindObjectBaseStopsAtRegionBoundary),
                                               TestCase_Make(Test_MemmyProcessFindObjectBaseNoCandidate),
                                               TestCase_Make(Test_MemmyProcessFindObjectBaseAmbiguous), );
