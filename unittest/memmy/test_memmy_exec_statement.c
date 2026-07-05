#include "test_memmy_common.h"

#include "memmy_exec.h"

typedef struct Test_ExecResultNode Test_ExecResultNode;
struct Test_ExecResultNode
{
    ListLink link;
    Memmy_ExecResult result;
};

typedef struct Test_ExecResultList Test_ExecResultList;
struct Test_ExecResultList
{
    Arena *arena;
    List list; // Test_ExecResultNode
};

static Memmy_Status Test_ExecResultSink_Push(void *user_data, Memmy_ExecResult *result)
{
    Test_ExecResultList *results = (Test_ExecResultList *)user_data;
    Test_ExecResultNode *node = Arena_PushStruct(results->arena, Test_ExecResultNode);
    node->result = *result;
    List_PushBack(&results->list, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_ExecResultSink Test_ExecResultSink(Test_ExecResultList *results, Arena *arena)
{
    *results = (Test_ExecResultList){
        .arena = arena,
    };
    return (Memmy_ExecResultSink){
        .callback = Test_ExecResultSink_Push,
        .user_data = results,
    };
}

static void Test_ParseStatement(Arena *arena, char *text, Memmy_Statement *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Statement_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static void Test_WriteLE(Test_MemmyBackend *backend, Memmy_Addr addr, U64 value, U64 size)
{
    U64 offset = addr - backend->memory_base;
    for (U64 i = 0; i < size; i++)
    {
        backend->memory[offset + i] = (U8)(value >> (i * 8));
    }
}

Test(Test_MemmyExecStatementProcsEmitsProcessResults)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&backend, 111, String8_Lit("first.exe"), String8_Lit("C:\\first.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&backend, 222, String8_Lit("second.exe"), String8_Lit("C:\\second.exe"),
                                 Memmy_PointerWidth_32);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "procs", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Statement_Execute(arena, &statement, (Memmy_ExecProcessSelection){0},
                                     Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_Ok);

    AssertEq(results.list.count, 2);
    Test_ExecResultNode *first = ContainerOf(results.list.first, Test_ExecResultNode, link);
    Test_ExecResultNode *second = ContainerOf(results.list.last, Test_ExecResultNode, link);
    AssertEq(first->result.kind, Memmy_ExecResultKind_Process);
    AssertEq(first->result.process.info.pid, 111);
    AssertStrEq(first->result.process.info.name, String8_Lit("first.exe"));
    AssertEq(second->result.kind, Memmy_ExecResultKind_Process);
    AssertEq(second->result.process.info.pid, 222);
    AssertEq(second->result.process.info.pointer_width, Memmy_PointerWidth_32);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementAddressEmitsStructuredAddress)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "<test-module.exe>+0x10", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 4242};
    AssertEq(Memmy_Statement_Execute(arena, &statement, selection, Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_Ok);

    AssertEq(results.list.count, 1);
    Test_ExecResultNode *node = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(node->result.kind, Memmy_ExecResultKind_Address);
    AssertEq(node->result.address.address, 0x10000010);
    AssertEq(node->result.address.pointer_width, Memmy_PointerWidth_64);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementPeekPokeEmitStructuredValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_WriteLE(&backend, 0x1010, 0x12345678, 4);
    Test_WriteLE(&backend, 0x1020, 1, 4);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 4242};

    Memmy_Statement peek_statement = {0};
    Test_ParseStatement(arena, "0x1010 : u32", &peek_statement);
    Test_ExecResultList peek_results = {0};
    Memmy_Error error = {0};
    AssertEq(
        Memmy_Statement_Execute(arena, &peek_statement, selection, Test_ExecResultSink(&peek_results, arena), &error),
        Memmy_Status_Ok);
    Test_ExecResultNode *peek = ContainerOf(peek_results.list.first, Test_ExecResultNode, link);
    AssertEq(peek->result.kind, Memmy_ExecResultKind_Peek);
    AssertEq(peek->result.peek.address, 0x1010);
    U8 expected[] = {0x78, 0x56, 0x34, 0x12};
    Test_AssertBytes(peek->result.peek.value.bytes, expected, sizeof(expected));

    Memmy_Statement poke_statement = {0};
    Test_ParseStatement(arena, "0x1020 : u32 = 1337", &poke_statement);
    Test_ExecResultList poke_results = {0};
    error = (Memmy_Error){0};
    AssertEq(
        Memmy_Statement_Execute(arena, &poke_statement, selection, Test_ExecResultSink(&poke_results, arena), &error),
        Memmy_Status_Ok);
    Test_ExecResultNode *poke = ContainerOf(poke_results.list.first, Test_ExecResultNode, link);
    AssertEq(poke->result.kind, Memmy_ExecResultKind_Poke);
    AssertEq(poke->result.poke.pid, 4242);
    AssertEq(poke->result.poke.address, 0x1020);
    AssertEq(backend.memory[0x20], 0x39);
    AssertEq(backend.memory[0x21], 0x05);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementScanEmitsMatchesAndSummary)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.memory[0x22] = 42;
    backend.memory[0x2a] = 42;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "0x1020:+0x10 : u8 == 42", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 4242};
    AssertEq(Memmy_Statement_Execute(arena, &statement, selection, Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_Ok);

    AssertEq(results.list.count, 3);
    Test_ExecResultNode *first = ContainerOf(results.list.first, Test_ExecResultNode, link);
    Test_ExecResultNode *second = ContainerOf(first->link.next, Test_ExecResultNode, link);
    Test_ExecResultNode *summary = ContainerOf(results.list.last, Test_ExecResultNode, link);
    AssertEq(first->result.kind, Memmy_ExecResultKind_Match);
    AssertEq(first->result.match.address, 0x1022);
    AssertEq(second->result.kind, Memmy_ExecResultKind_Match);
    AssertEq(second->result.match.address, 0x102a);
    AssertEq(summary->result.kind, Memmy_ExecResultKind_Summary);
    AssertEq(summary->result.summary.match_count, 2);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementExitEmitsControlResult)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "quit", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Statement_Execute(arena, &statement, (Memmy_ExecProcessSelection){0},
                                     Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_Ok);

    AssertEq(results.list.count, 1);
    Test_ExecResultNode *node = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(node->result.kind, Memmy_ExecResultKind_Control);
    AssertEq(node->result.control.kind, Memmy_ExecControlKind_Exit);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_statement =
    TestSuite_Make("Memmy Exec Statement", TestCase_Make(Test_MemmyExecStatementProcsEmitsProcessResults),
                   TestCase_Make(Test_MemmyExecStatementAddressEmitsStructuredAddress),
                   TestCase_Make(Test_MemmyExecStatementPeekPokeEmitStructuredValues),
                   TestCase_Make(Test_MemmyExecStatementScanEmitsMatchesAndSummary),
                   TestCase_Make(Test_MemmyExecStatementExitEmitsControlResult), );
