#include "memmy_exec.h"
#include "test_framework.h"

static U64 test_memmy_exec_address_list_process_call_count;

static Memmy_Status Test_MemmyExecAddress_ListProcesses(Arena *arena, Memmy_ProcessList *out, Memmy_Error *error)
{
    Unused(arena);
    Unused(out);
    Unused(error);

    test_memmy_exec_address_list_process_call_count++;
    return Memmy_Status_PlatformError;
}

static void Test_ParseAddress(Arena *arena, char *text, Memmy_AddressExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_AddressExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_ParseMemory(Arena *arena, char *text, Memmy_MemoryExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_AddModule(Arena *arena, Memmy_ModuleList *modules, String8 name, Memmy_Addr base)
{
    Memmy_Module *module = Memmy_ModuleList_Push(arena, modules);
    module->name = name;
    module->base = base;
    module->size = 0x1000;
}

Test(Test_MemmyExecAddressResolvesAbsoluteAddresses)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x000001d856780004", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(0, 0, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x000001d856780004);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesModuleBases)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x180000000);

    Memmy_Process process = {.pid = 1234};
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x180000000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesModulePlusMinusOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x1000);

    Memmy_Process process = {.pid = 1234};
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>+0x123-0x20", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1103);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesMemoryAddressExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x1000);

    Memmy_Process process = {.pid = 1234};
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemory(arena, "<client.dll>+0x123", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_MemoryExpr_ResolveAddress(&process, &modules, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1123);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressDetectsArithmeticOverflowAndUnderflow)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr overflow = {0};
    Memmy_AddressExpr underflow = {0};
    Test_ParseAddress(arena, "0xffffffffffffffff+1", &overflow);
    Test_ParseAddress(arena, "0x0-1", &underflow);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(0, 0, &overflow, &addr, &error), Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    error = (Memmy_Error){0};
    AssertEq(Memmy_AddressExpr_Resolve(0, 0, &underflow, &addr, &error), Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressReportsMissingModule)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("engine.dll"), 0x2000);

    Memmy_Process process = {.pid = 1234};
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_NotFound);
    AssertEq(error.status, Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressReportsAmbiguousModule)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x1000);
    Test_AddModule(arena, &modules, String8_Lit("CLIENT.dll"), 0x2000);

    Memmy_Process process = {.pid = 1234};
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressDoesNotEnumerateProcesses)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x4000);

    Memmy_Backend backend = {
        .name = String8_Lit("exec-address-test"),
        .capabilities = Memmy_BackendCap_ListProcs,
        .list_processes = Test_MemmyExecAddress_ListProcesses,
    };
    Memmy_Process process = {
        .backend = &backend,
        .pid = 1234,
        .pointer_width = Memmy_PointerWidth_64,
    };
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<game.exe!client.dll>+0x10", &expr);

    test_memmy_exec_address_list_process_call_count = 0;
    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x4010);
    AssertEq(test_memmy_exec_address_list_process_call_count, 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressRejectsMismatchedPidSelector)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ModuleList modules = {0};
    Test_AddModule(arena, &modules, String8_Lit("client.dll"), 0x4000);

    Memmy_Process process = {.pid = 1234};
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<5678!client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &modules, &expr, &addr, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_address =
    TestSuite_Make("Memmy Exec Address", TestCase_Make(Test_MemmyExecAddressResolvesAbsoluteAddresses),
                   TestCase_Make(Test_MemmyExecAddressResolvesModuleBases),
                   TestCase_Make(Test_MemmyExecAddressResolvesModulePlusMinusOffsets),
                   TestCase_Make(Test_MemmyExecAddressResolvesMemoryAddressExpressions),
                   TestCase_Make(Test_MemmyExecAddressDetectsArithmeticOverflowAndUnderflow),
                   TestCase_Make(Test_MemmyExecAddressReportsMissingModule),
                   TestCase_Make(Test_MemmyExecAddressReportsAmbiguousModule),
                   TestCase_Make(Test_MemmyExecAddressDoesNotEnumerateProcesses),
                   TestCase_Make(Test_MemmyExecAddressRejectsMismatchedPidSelector), );
