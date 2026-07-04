#include "memmy_exec.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

static void Test_MemmyExecPeekPoke_Parse(Arena *arena, char *text, Memmy_MemoryExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_MemmyExecPeekPoke_AddModule(Arena *arena, Memmy_ModuleList *modules, String8 name, Memmy_Addr base)
{
    Memmy_Module *module = Memmy_ModuleList_Push(arena, modules);
    module->name = name;
    module->base = base;
    module->size = 0x1000;
}

static Memmy_Process Test_MemmyExecPeekPoke_Process(Test_MemmyBackend *backend)
{
    return (Memmy_Process){
        .backend = Test_MemmyBackend_AsBackend(backend),
        .pid = 4242,
        .pointer_width = Memmy_PointerWidth_64,
        .backend_data = backend,
    };
}

static void Test_MemmyExecPeekPoke_WriteLE(Test_MemmyBackend *backend, Memmy_Addr addr, U64 value, U64 size)
{
    U64 offset = addr - backend->memory_base;
    for (U64 i = 0; i < size; i++)
    {
        backend->memory[offset + i] = (U8)(value >> (i * 8));
    }
}

Test(Test_MemmyExecPeekExecutesAbsoluteAddressPeek)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1010, 0x12345678, 4);

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1010 : u32", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.address, 0x1010);
    U8 expected[] = {0x78, 0x56, 0x34, 0x12};
    AssertEq(result.value.bytes.len, sizeof(expected));
    for (U64 i = 0; i < sizeof(expected); i++)
    {
        AssertEq(result.value.bytes.data[i], expected[i]);
    }

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekExecutesModuleAddressPeek)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1020, 0x7f, 1);

    Memmy_ModuleList modules = {0};
    Test_MemmyExecPeekPoke_AddModule(arena, &modules, String8_Lit("client.dll"), 0x1000);
    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "<client.dll>+0x20 : u8", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, &modules, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.address, 0x1020);
    AssertEq(result.value.bytes.len, 1);
    AssertEq(result.value.bytes.data[0], 0x7f);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekExecutesPointerChainPeek)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1000, 0x1040, 8);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1048, 0x2211, 2);

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1000->0x8 : u16", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.address, 0x1048);
    AssertEq(result.value.bytes.data[0], 0x11);
    AssertEq(result.value.bytes.data[1], 0x22);
    AssertEq(backend.read_call_count, 2);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekReadsStringUntilTerminator)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    U8 text[] = {'h', 'e', 'l', 'l', 'o', 0, 'x'};
    for (U64 i = 0; i < sizeof(text); i++)
    {
        backend.memory[0x40 + i] = text[i];
    }

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1040 : str", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertStrEq(result.value.bytes, String8_Lit("hello"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekReadsStringUntilNonPrintable)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    U8 text[] = {'a', 'b', '\n', 'c', 0};
    for (U64 i = 0; i < sizeof(text); i++)
    {
        backend.memory[0x50 + i] = text[i];
    }

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1050 : str", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertStrEq(result.value.bytes, String8_Lit("ab"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekReadsUtf8String)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    U8 text[] = {'c', 'a', 'f', 0xc3, 0xa9, 0, 'x'};
    for (U64 i = 0; i < sizeof(text); i++)
    {
        backend.memory[0x70 + i] = text[i];
    }

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1070 : str", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.value.bytes.len, 5);
    AssertEq(result.value.bytes.data[0], 'c');
    AssertEq(result.value.bytes.data[1], 'a');
    AssertEq(result.value.bytes.data[2], 'f');
    AssertEq(result.value.bytes.data[3], 0xc3);
    AssertEq(result.value.bytes.data[4], 0xa9);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekReadsWStringUntilTerminator)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    U8 text[] = {'H', 0, 'i', 0, 0, 0, 'x', 0};
    for (U64 i = 0; i < sizeof(text); i++)
    {
        backend.memory[0x60 + i] = text[i];
    }

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1060 : wstr", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.value.bytes.len, 4);
    AssertEq(result.value.bytes.data[0], 'H');
    AssertEq(result.value.bytes.data[1], 0);
    AssertEq(result.value.bytes.data[2], 'i');
    AssertEq(result.value.bytes.data[3], 0);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPokeExecutesAbsoluteAddressPoke)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1030, 1, 4);

    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1030 : u32 = 1337", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPokeResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePoke(arena, &process, 0, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.address, 0x1030);
    AssertEq(result.old_value.bytes.data[0], 1);
    AssertEq(backend.memory[0x30], 0x39);
    AssertEq(backend.memory[0x31], 0x05);
    AssertEq(backend.memory[0x32], 0x00);
    AssertEq(backend.memory[0x33], 0x00);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPokeExecutesModuleAddressPoke)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyExecPeekPoke_WriteLE(&backend, 0x1050, 2, 4);

    Memmy_ModuleList modules = {0};
    Test_MemmyExecPeekPoke_AddModule(arena, &modules, String8_Lit("client.dll"), 0x1000);
    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "<client.dll>+0x50 : i32 = -3", &expr);

    Memmy_Error error = {0};
    Memmy_ExecPokeResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePoke(arena, &process, &modules, &expr, &result, &error), Memmy_Status_Ok);
    AssertEq(result.address, 0x1050);
    AssertEq(backend.memory[0x50], 0xfd);
    AssertEq(backend.memory[0x51], 0xff);
    AssertEq(backend.memory[0x52], 0xff);
    AssertEq(backend.memory[0x53], 0xff);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPokeRejectsRhsAddressExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Error error = {0};
    Memmy_MemoryExpr expr = {0};

    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_Lit("0x1000 : u32 = <client.dll>+0x4"), &expr, &error),
             Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("value"));

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPeekPropagatesReadErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1000 : u32", &expr);

    Test_MemmyBackend_SetReadLimit(&backend, 2);
    Memmy_Error error = {0};
    Memmy_ExecPeekResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_PartialRead);
    AssertEq(error.status, Memmy_Status_PartialRead);

    Test_MemmyBackend_SetReadLimit(&backend, 0);
    Test_MemmyBackend_SetReadStatus(&backend, Memmy_Status_AccessDenied);
    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_ExecutePeek(arena, &process, 0, &expr, &result, &error), Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecPokePropagatesWriteErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Process process = Test_MemmyExecPeekPoke_Process(&backend);
    Memmy_MemoryExpr expr = {0};
    Test_MemmyExecPeekPoke_Parse(arena, "0x1000 : u32 = 7", &expr);

    Test_MemmyBackend_SetWriteLimit(&backend, 2);
    Memmy_Error error = {0};
    Memmy_ExecPokeResult result = {0};
    AssertEq(Memmy_MemoryExpr_ExecutePoke(arena, &process, 0, &expr, &result, &error), Memmy_Status_PartialWrite);
    AssertEq(error.status, Memmy_Status_PartialWrite);

    Test_MemmyBackend_SetWriteLimit(&backend, 0);
    Test_MemmyBackend_SetWriteStatus(&backend, Memmy_Status_AccessDenied);
    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_ExecutePoke(arena, &process, 0, &expr, &result, &error), Memmy_Status_AccessDenied);
    AssertEq(error.status, Memmy_Status_AccessDenied);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_peek_poke = TestSuite_Make(
    "Memmy Exec Peek Poke", TestCase_Make(Test_MemmyExecPeekExecutesAbsoluteAddressPeek),
    TestCase_Make(Test_MemmyExecPeekExecutesModuleAddressPeek),
    TestCase_Make(Test_MemmyExecPeekExecutesPointerChainPeek),
    TestCase_Make(Test_MemmyExecPeekReadsStringUntilTerminator),
    TestCase_Make(Test_MemmyExecPeekReadsStringUntilNonPrintable), TestCase_Make(Test_MemmyExecPeekReadsUtf8String),
    TestCase_Make(Test_MemmyExecPeekReadsWStringUntilTerminator),
    TestCase_Make(Test_MemmyExecPokeExecutesAbsoluteAddressPoke),
    TestCase_Make(Test_MemmyExecPokeExecutesModuleAddressPoke),
    TestCase_Make(Test_MemmyExecPokeRejectsRhsAddressExpressions),
    TestCase_Make(Test_MemmyExecPeekPropagatesReadErrors), TestCase_Make(Test_MemmyExecPokePropagatesWriteErrors), );
