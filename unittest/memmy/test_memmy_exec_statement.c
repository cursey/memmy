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

static Memmy_Status Test_ExecuteLine(Arena *arena, Memmy_ExecEnv *env, char *text, Memmy_ExecProcessSelection selection,
                                     Test_ExecResultList *results, Memmy_Error *error)
{
    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, text, &statement);
    return Memmy_Statement_ExecuteWithEnv(arena, env, &statement, selection, Test_ExecResultSink(results, arena),
                                          error);
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

Test(Test_MemmyExecStatementResolvesProcessQualifiedAbsoluteAddress)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "<4242!>0x1010", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Statement_Execute(arena, &statement, (Memmy_ExecProcessSelection){0},
                                     Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_Ok);

    AssertEq(backend.open_call_count, 1);
    AssertEq(backend.last_open_pid, 4242);
    Test_ExecResultNode *node = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(node->result.kind, Memmy_ExecResultKind_Address);
    AssertEq(node->result.address.address, 0x1010);
    AssertEq(node->result.address.pointer_width, Memmy_PointerWidth_64);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementRejectsProcessQualifiedAbsoluteAddressConflict)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 5678, String8_Lit("other-process"), String8_Lit("C:\\test\\other.exe"),
                                 Memmy_PointerWidth_64);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "<4242!>0x1010", &statement);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 5678};
    AssertEq(Memmy_Statement_Execute(arena, &statement, selection, Test_ExecResultSink(&results, arena), &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("cli"));
    AssertStrEq(error.message, String8_Lit("external process selector conflicts with expression process selector"));
    AssertEq(backend.open_call_count, 0);

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

Test(Test_MemmyExecStatementAssignsAndReassignsAddressVariables)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_Error error = {0};
    Test_ExecResultList results = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$addr = 0x1000", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    Test_ExecResultNode *assign = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(assign->result.kind, Memmy_ExecResultKind_Assignment);
    AssertStrEq(assign->result.assignment.name, String8_Lit("addr"));
    AssertEq(assign->result.assignment.variable_kind, Memmy_VariableExprKind_Address);

    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$addr = 0x1010", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$addr+0x2", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);

    Test_ExecResultNode *address = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(address->result.kind, Memmy_ExecResultKind_Address);
    AssertEq(address->result.address.address, 0x1012);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementResolvesVariablesInAddressPeekAndConstOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.memory[0x23] = 0xab;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 4242};
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$base = 0x1000", selection, &results, &error), Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$off = (0x23)", selection, &results, &error), Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$base+$off : u8", selection, &results, &error), Memmy_Status_Ok);

    Test_ExecResultNode *peek = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(peek->result.kind, Memmy_ExecResultKind_Peek);
    AssertEq(peek->result.peek.address, 0x1023);
    AssertEq(peek->result.peek.value.bytes.data[0], 0xab);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementResolvesModuleAddressVariables)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$base = <test-process!test-module.exe>", (Memmy_ExecProcessSelection){0},
                              &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$base+0x123", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);

    Test_ExecResultNode *address = ContainerOf(results.list.first, Test_ExecResultNode, link);
    AssertEq(address->result.kind, Memmy_ExecResultKind_Address);
    AssertEq(address->result.address.address, 0x10000123);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementResolvesRangeVariables)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    backend.memory[0x2a] = 42;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_ExecProcessSelection selection = {.has_pid = 1, .pid = 4242};
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$range = 0x1000:+0x40", selection, &results, &error), Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$range : u8 == 42", selection, &results, &error), Memmy_Status_Ok);

    AssertTrue(results.list.count >= 2);
    Test_ExecResultNode *summary = ContainerOf(results.list.last, Test_ExecResultNode, link);
    AssertEq(summary->result.kind, Memmy_ExecResultKind_Summary);
    AssertTrue(summary->result.summary.match_count >= 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementResolvesWholeProcessBracketRangeVariables)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_SetMemoryBase(&backend, 0);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0, 0x100, Memmy_RegionAccess_Read | Memmy_RegionAccess_Write,
                                Memmy_RegionState_Committed);
    backend.memory[0x2a] = 42;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Test_ExecResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$range = <test-process!>[0x20:+0x20]", (Memmy_ExecProcessSelection){0},
                              &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$range : u8 == 42", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);

    Test_ExecResultNode *summary = ContainerOf(results.list.last, Test_ExecResultNode, link);
    AssertEq(summary->result.kind, Memmy_ExecResultKind_Summary);
    AssertTrue(summary->result.summary.match_count >= 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementVarsAndUnset)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_Error error = {0};
    Test_ExecResultList results = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$addr = 0x1000", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$count = (2)", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "vars", (Memmy_ExecProcessSelection){0}, &results, &error), Memmy_Status_Ok);
    AssertEq(results.list.count, 2);

    B32 saw_addr = 0;
    B32 saw_count = 0;
    List_ForEach(Test_ExecResultNode, node, &results.list, link)
    {
        if (String8_Eq(node->result.variable_binding.name, String8_Lit("addr")))
        {
            saw_addr = 1;
            AssertEq(node->result.variable_binding.variable_kind, Memmy_VariableExprKind_Address);
        }
        if (String8_Eq(node->result.variable_binding.name, String8_Lit("count")))
        {
            saw_count = 1;
            AssertEq(node->result.variable_binding.variable_kind, Memmy_VariableExprKind_Const);
        }
    }
    AssertTrue(saw_addr);
    AssertTrue(saw_count);

    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "unset $addr", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "vars", (Memmy_ExecProcessSelection){0}, &results, &error), Memmy_Status_Ok);
    AssertEq(results.list.count, 1);

    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "unset $missing", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementRejectsWrongKindVariables)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_Error error = {0};
    Test_ExecResultList results = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$range = 0x1000:+0x10", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$range+0x4", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_InvalidArgument);
    AssertTrue(String8_Find(error.message, String8_Lit("expected address"), 0) != STRING8_NPOS);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecStatementRejectsVariableCycles)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecEnv env = Memmy_ExecEnv_Create(arena);
    Memmy_Error error = {0};
    Test_ExecResultList results = {0};

    AssertEq(Test_ExecuteLine(arena, &env, "$a = $b+0", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$b = $a+0", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_Ok);
    results = (Test_ExecResultList){0};
    AssertEq(Test_ExecuteLine(arena, &env, "$a", (Memmy_ExecProcessSelection){0}, &results, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.message, String8_Lit("variable cycle: $a -> $b -> $a"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_statement = TestSuite_Make(
    "Memmy Exec Statement", TestCase_Make(Test_MemmyExecStatementProcsEmitsProcessResults),
    TestCase_Make(Test_MemmyExecStatementAddressEmitsStructuredAddress),
    TestCase_Make(Test_MemmyExecStatementResolvesProcessQualifiedAbsoluteAddress),
    TestCase_Make(Test_MemmyExecStatementRejectsProcessQualifiedAbsoluteAddressConflict),
    TestCase_Make(Test_MemmyExecStatementPeekPokeEmitStructuredValues),
    TestCase_Make(Test_MemmyExecStatementScanEmitsMatchesAndSummary),
    TestCase_Make(Test_MemmyExecStatementExitEmitsControlResult),
    TestCase_Make(Test_MemmyExecStatementAssignsAndReassignsAddressVariables),
    TestCase_Make(Test_MemmyExecStatementResolvesVariablesInAddressPeekAndConstOffsets),
    TestCase_Make(Test_MemmyExecStatementResolvesModuleAddressVariables),
    TestCase_Make(Test_MemmyExecStatementResolvesRangeVariables),
    TestCase_Make(Test_MemmyExecStatementResolvesWholeProcessBracketRangeVariables),
    TestCase_Make(Test_MemmyExecStatementVarsAndUnset), TestCase_Make(Test_MemmyExecStatementRejectsWrongKindVariables),
    TestCase_Make(Test_MemmyExecStatementRejectsVariableCycles), );
