#include "base_os.h"
#include "memmy.h"
#include "memmy_cli.h"
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

Test(Test_MemmyParseAddressAcceptsUnsignedTokens)
{
    Memmy_Addr addr = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseAddress(String8_Lit("0x000001d856780004"), &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x000001d856780004ull);

    AssertEq(Memmy_ParseAddress(String8_Lit("0X1000"), &addr, &error), Memmy_Status_Ok);
    AssertEq(addr, 0x1000);

    AssertEq(Memmy_ParseAddress(String8_Lit("4096"), &addr, &error), Memmy_Status_Ok);
    AssertEq(addr, 4096);
}

Test(Test_MemmyParseAddressRejectsExpressionsAndNames)
{
    String8 rejected[] = {
        String8_Lit("-1"),         String8_Lit("+1"), String8_Lit("0x1000+4"), String8_Lit("(0x1000)"),
        String8_Lit("client.dll"), String8_Lit("0x"), String8_Lit(""),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_Addr addr = 123;
        Memmy_Error error = {0};
        AssertEq(Memmy_ParseAddress(rejected[i], &addr, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("address"));
        AssertStrEq(error.input, rejected[i]);
        AssertEq(addr, 123);
    }
}

Test(Test_MemmyParseAddressRejectsOverflow)
{
    Memmy_Addr addr = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseAddress(String8_Lit("18446744073709551616"), &addr, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));

    AssertEq(Memmy_ParseAddress(String8_Lit("0x10000000000000000"), &addr, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("address"));
}

Test(Test_MemmyParseSizeAcceptsDecimalAndHexAndRejectsOverflow)
{
    Memmy_Size size = 0;
    Memmy_Error error = {0};

    AssertEq(Memmy_ParseSize(String8_Lit("4096"), &size, &error), Memmy_Status_Ok);
    AssertEq(size, 4096);

    AssertEq(Memmy_ParseSize(String8_Lit("0x1000"), &size, &error), Memmy_Status_Ok);
    AssertEq(size, 0x1000);

    AssertEq(Memmy_ParseSize(String8_Lit("18446744073709551616"), &size, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyRangeFromStartEndValidatesOrder)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Range_FromStartEnd(0x1000, 0x2000, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x2000);

    AssertEq(Memmy_Range_FromStartEnd(0x1000, 0x1000, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1000);

    AssertEq(Memmy_Range_FromStartEnd(0x2000, 0x1000, &range, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyRangeFromStartLengthAllowsZeroAndRejectsOverflow)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Range_FromStartLength(0x1000, 0, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1000);

    AssertEq(Memmy_Range_FromStartLength(0x1000, 0x20, &range, &error), Memmy_Status_Ok);
    AssertEq(range.end, 0x1020);

    AssertEq(Memmy_Range_FromStartLength(U64_MAX, 1, &range, &error), Memmy_Status_Overflow);
    AssertStrEq(error.context, String8_Lit("range"));
}

Test(Test_MemmyCliParseRangeOptionsAcceptsValidShapes)
{
    Memmy_Range range = {0};
    Memmy_Error error = {0};
    char *start_end[] = {"--start", "0x1000", "--end", "0x1020"};
    char *start_length[] = {"--start", "0x1000", "--length", "32"};

    AssertEq(Memmy_Cli_ParseRangeOptions((I32)ArrayCount(start_end), start_end, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1020);

    AssertEq(Memmy_Cli_ParseRangeOptions((I32)ArrayCount(start_length), start_length, &range, &error), Memmy_Status_Ok);
    AssertEq(range.start, 0x1000);
    AssertEq(range.end, 0x1020);
}

Test(Test_MemmyCliParseRangeOptionsRejectsInvalidCombinations)
{
    char *missing_start[] = {"--end", "0x1020"};
    char *both_end_length[] = {"--start", "0x1000", "--end", "0x1020", "--length", "0x20"};
    char *neither_end_length[] = {"--start", "0x1000"};
    char *missing_start_value[] = {"--start", "--end", "0x1020"};
    char *missing_end_value[] = {"--start", "0x1000", "--end"};
    char *duplicate_start[] = {"--start", "0x1000", "--start", "0x1000", "--end", "0x1020"};
    char *end_before_start[] = {"--start", "0x2000", "--end", "0x1000"};
    char *bad_length[] = {"--start", "0x1000", "--length", "-1"};

    struct
    {
        char **argv;
        I32 argc;
        Memmy_Status status;
    } cases[] = {
        {missing_start, (I32)ArrayCount(missing_start), Memmy_Status_ParseError},
        {both_end_length, (I32)ArrayCount(both_end_length), Memmy_Status_ParseError},
        {neither_end_length, (I32)ArrayCount(neither_end_length), Memmy_Status_ParseError},
        {missing_start_value, (I32)ArrayCount(missing_start_value), Memmy_Status_ParseError},
        {missing_end_value, (I32)ArrayCount(missing_end_value), Memmy_Status_ParseError},
        {duplicate_start, (I32)ArrayCount(duplicate_start), Memmy_Status_ParseError},
        {end_before_start, (I32)ArrayCount(end_before_start), Memmy_Status_InvalidArgument},
        {bad_length, (I32)ArrayCount(bad_length), Memmy_Status_ParseError},
    };

    for (U64 i = 0; i < ArrayCount(cases); i++)
    {
        Memmy_Range range = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Cli_ParseRangeOptions(cases[i].argc, cases[i].argv, &range, &error), cases[i].status);
        AssertTrue(error.status != Memmy_Status_Ok);
    }
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
    AssertEq(Memmy_Process_Open(arena, 222, Memmy_ProcessAccess_Query, &process, 0), Memmy_Status_Ok);
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

Test(Test_MemmyCliHelpAndVersion)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *help_argv[] = {"memmy", "--help"};
    char *version_argv[] = {"memmy", "--version"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(help_argv), help_argv, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("procs"), 0) != STRING8_NPOS);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(version_argv), version_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("memmy 0.0.0\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliProcsModsRegionsTextOutput)
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
    Test_MemmyBackend_AddModule(&test_backend, 222, String8_Lit("beta.dll"), String8_Lit("C:\\beta.dll"),
                                0x00007ff800000000, 0x1a4000);
    Test_MemmyBackend_AddRegion(&test_backend, 222, 0x000001d800000000, 0x10000,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *procs_argv[] = {"memmy", "procs", "--filter", "beta"};
    char *mods_argv[] = {"memmy", "mods", "--pid", "222", "--filter", "beta"};
    char *regions_argv[] = {"memmy", "regions", "--pid", "222"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(procs_argv), procs_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("PID     ARCH   NAME\n222    x64    beta.exe\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(mods_argv), mods_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                SIZE        NAME\n"
                                 "0x00007ff800000000  0x1a4000    beta.dll\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(regions_argv), regions_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"
                                 "0x000001d800000000  0x000001d800010000  0x10000     rw-     committed\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliNameAmbiguityAndRegionOverflow)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    test_backend.module_count = 0;
    test_backend.region_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 111, String8_Lit("same.exe"), String8_Lit("C:\\one\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("same.exe"), String8_Lit("C:\\two\\same.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&test_backend, 333, String8_Lit("overflow.exe"), String8_Lit("C:\\overflow.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(&test_backend, 333, U64_MAX, 1, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *ambiguous_argv[] = {"memmy", "mods", "--name", "same.exe"};
    char *overflow_argv[] = {"memmy", "regions", "--pid", "333"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ambiguous_argv), ambiguous_argv, &out, &error),
             Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(overflow_argv), overflow_argv, &out, &error),
             Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliExitCodeMapping)
{
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ok), 0);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_ParseError), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidArgument), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Overflow), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_InvalidEncoding), 2);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_NotFound), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Ambiguous), 3);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_AccessDenied), 4);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialRead), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PartialWrite), 5);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unsupported), 6);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_PlatformError), 7);
    AssertEq(Memmy_Cli_ExitCodeFromStatus(Memmy_Status_Unreadable), 1);
}

Test(Test_MemmyDefaultContextWin32ReadWriteCallbacks)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS
    AssertEq(status, Memmy_Status_Ok);
    AssertTrue(ctx.backend != 0);
    AssertTrue(Memmy_Backend_HasCapability(ctx.backend, Memmy_BackendCap_Read));
    AssertTrue(Memmy_Backend_HasCapability(ctx.backend, Memmy_BackendCap_Write));
    AssertTrue(ctx.backend->read != 0);
    AssertTrue(ctx.backend->write != 0);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

Test(Test_MemmyDefaultContextWin32ReadWriteSelfProcess)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};

    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    volatile U32 value = 0x11223344;
    U32 read_value = 0;
    U32 write_value = 0x55667788;
    U64 byte_count = 0;
    Memmy_Process *process = 0;

    AssertEq(Memmy_Process_Open(arena, Os_GetProcessId(), Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write,
                                &process, &error),
             Memmy_Status_Ok);
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

TestSuite suite_memmy = TestSuite_Make(
    "Memmy", TestCase_Make(Test_MemmyHeaderExportsBaseTypes), TestCase_Make(Test_MemmyStatusAndErrorHelpers),
    TestCase_Make(Test_MemmyParseAddressAcceptsUnsignedTokens),
    TestCase_Make(Test_MemmyParseAddressRejectsExpressionsAndNames),
    TestCase_Make(Test_MemmyParseAddressRejectsOverflow),
    TestCase_Make(Test_MemmyParseSizeAcceptsDecimalAndHexAndRejectsOverflow),
    TestCase_Make(Test_MemmyRangeFromStartEndValidatesOrder),
    TestCase_Make(Test_MemmyRangeFromStartLengthAllowsZeroAndRejectsOverflow),
    TestCase_Make(Test_MemmyCliParseRangeOptionsAcceptsValidShapes),
    TestCase_Make(Test_MemmyCliParseRangeOptionsRejectsInvalidCombinations), TestCase_Make(Test_MemmyContextSetPushPop),
    TestCase_Make(Test_MemmyDispatchRejectsMissingContextAndBackend),
    TestCase_Make(Test_MemmyDispatchRejectsMissingCallback),
    TestCase_Make(Test_MemmyCloseMarksProcessClosedWithoutCallback),
    TestCase_Make(Test_MemmyTestBackendCapabilitiesAndReadWrite),
    TestCase_Make(Test_MemmyTestBackendConfiguredInventory), TestCase_Make(Test_MemmyCliHelpAndVersion),
    TestCase_Make(Test_MemmyCliProcsModsRegionsTextOutput), TestCase_Make(Test_MemmyCliNameAmbiguityAndRegionOverflow),
    TestCase_Make(Test_MemmyCliExitCodeMapping), TestCase_Make(Test_MemmyDefaultContextWin32ReadWriteCallbacks),
    TestCase_Make(Test_MemmyDefaultContextWin32ReadWriteSelfProcess), );
