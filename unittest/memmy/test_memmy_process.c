#include "test_memmy_common.h"

Test(Test_MemmyTestBackendReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Backend *backend = Test_MemmyBackend_AsBackend(&test_backend);
    Memmy_Context ctx = {.backend = backend};
    Memmy_Context_Set(&ctx);

    Memmy_ProcessList processes = {0};
    AssertEq(Memmy_ListProcesses(arena, &processes, 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 1);
    Memmy_ProcessInfo *info = ContainerOf(processes.list.first, Memmy_ProcessInfo, link);
    AssertEq(info->pid, 4242);

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

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Memmy_RegionList regions = {0};
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, 0), Memmy_Status_Ok);
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
    Memmy_ProcessList processes = {0};
    test_backend.backend.list_processes = 0;
    AssertEq(Memmy_ListProcesses(arena, &processes, &error), Memmy_Status_Unsupported);

    U8 buffer[4] = {0};
    U64 byte_count = 0;
    test_backend.backend.read = 0;
    AssertEq(Memmy_Process_Read(process, test_backend.memory_base, buffer, sizeof(buffer), &byte_count, &error),
             Memmy_Status_Unsupported);

    test_backend.backend.write = 0;
    AssertEq(Memmy_Process_Write(process, test_backend.memory_base, buffer, sizeof(buffer), &byte_count, &error),
             Memmy_Status_Unsupported);

    Memmy_ModuleList modules = {0};
    test_backend.backend.list_modules = 0;
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, &error), Memmy_Status_Unsupported);

    Memmy_RegionList regions = {0};
    test_backend.backend.list_regions = 0;
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, &error), Memmy_Status_Unsupported);

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

    Memmy_ProcessList processes = {0};
    AssertEq(Memmy_ListProcesses(arena, &processes, 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 2);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 222, &process, 0), Memmy_Status_Ok);
    AssertEq(process->pointer_width, Memmy_PointerWidth_64);

    Memmy_ModuleList modules = {0};
    AssertEq(Memmy_Process_ListModules(arena, process, &modules, 0), Memmy_Status_Ok);
    AssertEq(modules.list.count, 1);

    Memmy_RegionList regions = {0};
    AssertEq(Memmy_Process_ListRegions(arena, process, &regions, 0), Memmy_Status_Ok);
    AssertEq(regions.list.count, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}
TestSuite suite_memmy_process = TestSuite_Make("Memmy Process", TestCase_Make(Test_MemmyTestBackendReadWrite),
                                               TestCase_Make(Test_MemmyProcessMissingBackendCallbacksReturnUnsupported),
                                               TestCase_Make(Test_MemmyProcessReadDispatchAndFailureMapping),
                                               TestCase_Make(Test_MemmyProcessWriteDispatchAndFailureMapping),
                                               TestCase_Make(Test_MemmyTestBackendConfiguredInventory), );
