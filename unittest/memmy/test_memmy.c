#include "memmy.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

Test(Test_MemmyHeaderExportsBaseTypes)
{
    U64 value = 42;
    AssertEq(value, 42);

    Memmy_Addr addr = 0x1000;
    Memmy_Size size = 16;
    Memmy_ProcessList processes = {0};
    Memmy_ModuleList modules = {0};
    Memmy_RegionList regions = {0};
    AssertEq(addr, 0x1000);
    AssertEq(size, 16);
    AssertEq(processes.list.count, 0);
    AssertEq(modules.list.count, 0);
    AssertEq(regions.list.count, 0);
}

Test(Test_MemmyStatusAndErrorHelpers)
{
    AssertStrEq(Memmy_Status_String(Memmy_Status_ParseError), String8_Lit("parse_error"));
    AssertStrEq(Memmy_Status_String((Memmy_Status)9999), String8_Lit("unknown"));

    Memmy_Error error = {0};
    Memmy_Error_Set(&error, Memmy_Status_Unsupported, String8_Lit("backend"), String8_Lit("no callback"));
    AssertEq(error.status, Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));
    AssertStrEq(error.message, String8_Lit("no callback"));
}

Test(Test_MemmyContextSetPushPop)
{
    Memmy_Context ctx_a = {0};
    Memmy_Context ctx_b = {0};

    Memmy_Context_Set(&ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_a);

    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx_b);
    AssertTrue(old_ctx == &ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_b);

    Memmy_Context_Pop(old_ctx);
    AssertTrue(Memmy_Context_Get() == &ctx_a);
    Memmy_Context_Set(0);
}

Test(Test_MemmyDispatchRejectsMissingContextAndBackend)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ProcessList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(0);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context ctx = {0};
    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyDispatchRejectsMissingCallback)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Backend backend = {.name = String8_Lit("empty")};
    Memmy_Context ctx = {.backend = &backend};
    Memmy_ProcessList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_ListProcesses(arena, &list, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Process process = {.backend = &backend};
    U8 buffer[4] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(&process, 0, buffer, sizeof(buffer), &bytes_read, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCloseMarksProcessClosedWithoutCallback)
{
    Memmy_Backend backend = {.name = String8_Lit("no-close")};
    Memmy_Process process = {.backend = &backend};

    AssertTrue(Memmy_Process_IsOpen(&process));
    Memmy_Process_Close(&process);
    AssertTrue(!Memmy_Process_IsOpen(&process));
}

Test(Test_MemmyTestBackendCapabilitiesAndReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Backend *backend = Test_MemmyBackend_AsBackend(&test_backend);
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_Read));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_Write));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListProcs));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListModules));
    AssertTrue(Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListRegions));

    Memmy_Context ctx = {.backend = backend};
    Memmy_Context_Set(&ctx);

    Memmy_ProcessList processes = {0};
    AssertEq(Memmy_ListProcesses(arena, &processes, 0), Memmy_Status_Ok);
    AssertEq(processes.list.count, 1);
    Memmy_ProcessInfo *info = ContainerOf(processes.list.first, Memmy_ProcessInfo, link);
    AssertEq(info->pid, 4242);

    Memmy_Process *process = 0;
    AssertEq(Memmy_Process_Open(arena, 4242, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write, &process, 0),
             Memmy_Status_Ok);
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

TestSuite suite_memmy = TestSuite_Make(
    "Memmy", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), TestCase_Make(Test_MemmyStatusAndErrorHelpers),
    TestCase_Make(Test_MemmyContextSetPushPop), TestCase_Make(Test_MemmyDispatchRejectsMissingContextAndBackend),
    TestCase_Make(Test_MemmyDispatchRejectsMissingCallback),
    TestCase_Make(Test_MemmyCloseMarksProcessClosedWithoutCallback),
    TestCase_Make(Test_MemmyTestBackendCapabilitiesAndReadWrite), );
