#include "memmy_exec.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

static U64 test_memmy_exec_address_list_process_call_count;

static Memmy_Status Test_MemmyExecAddress_EnumerateProcesses(Arena *arena, Memmy_ProcessInfoSink sink,
                                                             Memmy_Error *error)
{
    Unused(arena);
    Unused(sink);
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

static void Test_AddModule(Test_MemmyBackend *backend, String8 name, Memmy_Addr base)
{
    Test_MemmyBackend_AddModule(backend, 4242, name, String8_Lit(""), base, 0x1000);
}

static Memmy_Process Test_ProcessForBackend(Test_MemmyBackend *backend, Memmy_PointerWidth pointer_width)
{
    return (Memmy_Process){
        .backend = Test_MemmyBackend_AsBackend(backend),
        .pid = 4242,
        .pointer_width = pointer_width,
        .backend_data = backend,
    };
}

static void Test_WriteU64LE(Test_MemmyBackend *backend, Memmy_Addr addr, U64 value, U64 size)
{
    U64 offset = addr - backend->memory_base;
    for (U64 i = 0; i < size; i++)
    {
        backend->memory[offset + i] = (U8)(value >> (i * 8));
    }
}

Test(Test_MemmyExecAddressResolvesAbsoluteAddresses)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x000001d856780004", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(0, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x000001d856780004);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesModuleBases)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x180000000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x180000000);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesModulePlusMinusOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x1000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>+0x123-0x20", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1103);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesMemoryAddressExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x1000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_MemoryExpr expr = {0};
    Test_ParseMemory(arena, "<client.dll>+0x123", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_MemoryExpr_ResolveAddress(&process, &expr, &addr, &error), Memmy_Status_Ok);
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
    AssertEq(Memmy_AddressExpr_Resolve(0, &overflow, &addr, &error), Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    error = (Memmy_Error){0};
    AssertEq(Memmy_AddressExpr_Resolve(0, &underflow, &addr, &error), Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesBareDereference)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteU64LE(&backend, 0x1000, 0x1080, 8);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1080);
    AssertEq(backend.read_call_count, 1);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesDereferencePlusOffset)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_SetMemoryBase(&backend, 0x1123);
    Test_WriteU64LE(&backend, 0x1123, 0x2000, 8);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x1000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>+0x123->0x8", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x2008);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressResolvesChainedDereferences)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteU64LE(&backend, 0x1000, 0x1020, 8);
    Test_WriteU64LE(&backend, 0x1030, 0x1050, 8);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->0x10->", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1050);
    AssertEq(backend.read_call_count, 2);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressUses32BitPointerWidth)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteU64LE(&backend, 0x1000, 0xffffffff12345678ull, 8);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_32);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x12345678);
    AssertTrue(backend.max_read_end == 0x1004);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressUses64BitPointerWidth)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteU64LE(&backend, 0x1000, 0x1122334455667788ull, 8);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x1122334455667788ull);
    AssertTrue(backend.max_read_end == 0x1008);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressPropagatesPointerReadFailures)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->", &expr);

    Test_MemmyBackend_AddUnreadableRange(&backend, 0x1000, 0x1008);
    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Unreadable);
    AssertEq(error.status, Memmy_Status_Unreadable);

    backend.unreadable_range_count = 0;
    Test_MemmyBackend_SetReadLimit(&backend, 2);
    error = (Memmy_Error){0};
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_PartialRead);
    AssertEq(error.status, Memmy_Status_PartialRead);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressDetectsOverflowAfterPointerRead)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteU64LE(&backend, 0x1000, U64_MAX, 8);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "0x1000->1", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Overflow);
    AssertEq(error.status, Memmy_Status_Overflow);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressReportsMissingModule)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("engine.dll"), 0x2000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_NotFound);
    AssertEq(error.status, Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressReportsAmbiguousModule)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x1000);
    Test_AddModule(&backend, String8_Lit("CLIENT.dll"), 0x2000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ambiguous);
    AssertEq(error.status, Memmy_Status_Ambiguous);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressDoesNotEnumerateProcesses)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.module_count = 0;
    Test_AddModule(&test_backend, String8_Lit("client.dll"), 0x4000);

    Memmy_Backend backend = test_backend.backend;
    backend.name = String8_Lit("exec-address-test");
    backend.enumerate_processes = Test_MemmyExecAddress_EnumerateProcesses;
    Memmy_Process process = {
        .backend = &backend,
        .pid = 4242,
        .pointer_width = Memmy_PointerWidth_64,
        .backend_data = &test_backend,
    };
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<game.exe!client.dll>+0x10", &expr);

    test_memmy_exec_address_list_process_call_count = 0;
    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_Ok);
    AssertTrue(addr == 0x4010);
    AssertEq(test_memmy_exec_address_list_process_call_count, 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecAddressRejectsMismatchedPidSelector)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.module_count = 0;
    Test_AddModule(&backend, String8_Lit("client.dll"), 0x4000);

    Memmy_Process process = Test_ProcessForBackend(&backend, Memmy_PointerWidth_64);
    Memmy_AddressExpr expr = {0};
    Test_ParseAddress(arena, "<5678!client.dll>", &expr);

    Memmy_Error error = {0};
    Memmy_Addr addr = 0;
    AssertEq(Memmy_AddressExpr_Resolve(&process, &expr, &addr, &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_address =
    TestSuite_Make("Memmy Exec Address", TestCase_Make(Test_MemmyExecAddressResolvesAbsoluteAddresses),
                   TestCase_Make(Test_MemmyExecAddressResolvesModuleBases),
                   TestCase_Make(Test_MemmyExecAddressResolvesModulePlusMinusOffsets),
                   TestCase_Make(Test_MemmyExecAddressResolvesMemoryAddressExpressions),
                   TestCase_Make(Test_MemmyExecAddressDetectsArithmeticOverflowAndUnderflow),
                   TestCase_Make(Test_MemmyExecAddressResolvesBareDereference),
                   TestCase_Make(Test_MemmyExecAddressResolvesDereferencePlusOffset),
                   TestCase_Make(Test_MemmyExecAddressResolvesChainedDereferences),
                   TestCase_Make(Test_MemmyExecAddressUses32BitPointerWidth),
                   TestCase_Make(Test_MemmyExecAddressUses64BitPointerWidth),
                   TestCase_Make(Test_MemmyExecAddressPropagatesPointerReadFailures),
                   TestCase_Make(Test_MemmyExecAddressDetectsOverflowAfterPointerRead),
                   TestCase_Make(Test_MemmyExecAddressReportsMissingModule),
                   TestCase_Make(Test_MemmyExecAddressReportsAmbiguousModule),
                   TestCase_Make(Test_MemmyExecAddressDoesNotEnumerateProcesses),
                   TestCase_Make(Test_MemmyExecAddressRejectsMismatchedPidSelector), );
