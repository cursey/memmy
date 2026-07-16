#include "test_memmy_eval_common.h"

Test(Test_MemmyEvalModuleTargetAndProcessRangeResolve)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);

    MemmyEval_Value module = {0};
    Test_EvalExprText(env, arena, "<test-module.exe>", &module);
    AssertEq(module.kind, MemmyEval_ValueKind_Range);
    AssertEq(module.range.start, 0x10000000);
    AssertEq(module.range.end, 0x10002000);

    MemmyEval_Value process_range = {0};
    Test_EvalExprText(env, arena, "[0..]", &process_range);
    AssertEq(process_range.kind, MemmyEval_ValueKind_ProcessRange);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalModuleTargetVariableStoresPlainAddress)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    Test_EvalStatementText(env, arena, "$addr = <test-module.exe>+0x4");
    AssertEq(backend.open_call_count, 1);
    AssertEq(backend.close_call_count, 1);

    MemmyEval_Value addr = {0};
    AssertEq(MemmyEval_Env_Find(env, String8_Lit("addr"), &addr), Memmy_Status_Ok);
    AssertEq(addr.kind, MemmyEval_ValueKind_Address);
    AssertEq(addr.address, 0x10000004);

    Test_MemmyBackend_SetMemoryBase(&backend, 0x10000000);
    MemmyEval_Value read = {0};
    Test_EvalStatementResult(env, arena, "$addr as u32", &read);
    AssertEq(read.kind, MemmyEval_ValueKind_TypedValue);
    AssertEq(read.address, 0x10000004);
    AssertEq(read.constant, 0x07060504);
    AssertEq(backend.open_call_count, 2);
    AssertEq(backend.close_call_count, 2);
    AssertTrue(MemmyEval_Env_GetDefaultProcess(env, 0, 0));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalModuleAddressArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\test\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("a.dll"), String8_Lit("C:\\test\\a.dll"), 0x1000, 0x100);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x2000,
                                0x100);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("b.dll"), String8_Lit("C:\\test\\b.dll"), 0x2800, 0x100);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "<client.dll> + 0x123", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x2123);

    Test_EvalExprText(env, arena, "0x123 + <client.dll>", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x2123);

    Test_EvalExprText(env, arena, "<client.dll> - 0x10", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, 0x1ff0);

    Test_EvalExprText(env, arena, "(<client.dll> + 0x123) - <client.dll>", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Const);
    AssertEq(value.constant, 0x123);

    Test_EvalExprText(env, arena, "<b.dll> - <a.dll>", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Const);
    AssertEq(value.constant, 0x1800);

    Test_EvalExprText(env, arena, "[<client.dll>+0x10..+0x20]", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Range);
    AssertEq(value.range.start, 0x2010);
    AssertEq(value.range.end, 0x2030);

    Test_EvalExprText(env, arena, "[@0x2000 + 0x10..+0x20]", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Range);
    AssertEq(value.range.start, 0x2010);
    AssertEq(value.range.end, 0x2030);

    MemmyAst_Node *expr = 0;
    Memmy_Error error = {0};
    Test_EvalParseExpr(arena, "0x123 - <client.dll>", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);

    error = (Memmy_Error){0};
    Test_EvalParseExpr(arena, "<client.dll> + <b.dll>", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalStoredAddressReadUsesCurrentSelectedProcess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("a.exe"), String8_Lit("C:\\test\\a.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddProcess(&backend, 5678, String8_Lit("b.exe"), String8_Lit("C:\\test\\b.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x10000000, 0x100);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);
    Test_EvalStatementText(env, arena, "$addr = <client.dll>+0x4");

    Test_MemmyBackend_SetMemoryBase(&backend, 0x10000000);
    MemmyEval_Env_SetDefaultProcess(env, 5678, Memmy_PointerWidth_64);
    MemmyEval_Value read = {0};
    Test_EvalStatementResult(env, arena, "$addr as u32", &read);
    AssertEq(read.kind, MemmyEval_ValueKind_TypedValue);
    AssertEq(read.address, 0x10000004);
    AssertEq(read.constant, 0x07060504);
    AssertEq(backend.last_open_pid, 5678);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalAddressFromTypedValueUsesAmbientProcess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_SetMemoryBase(&backend, 0x10000000);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    U64 pointer = 0x10000020;
    for (U64 i = 0; i < 8; i++)
    {
        backend.memory[i] = (U8)(pointer >> (i * 8));
    }
    backend.memory[0x20] = 42;

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    MemmyEval_Value read = {0};
    Test_EvalExprText(env, arena, "@(<test-module.exe>+0 as ptr) as u8", &read);
    AssertEq(read.kind, MemmyEval_ValueKind_TypedValue);
    AssertEq(read.address, 0x10000020);
    AssertEq(read.constant, 42);
    AssertEq(backend.open_call_count, 1);
    AssertEq(backend.close_call_count, 1);
    AssertEq(backend.read_call_count, 2);
    AssertTrue(MemmyEval_Env_GetDefaultProcess(env, 0, 0));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalRangeEndpointsDoNotCarryProcessProvenance)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("a.exe"), String8_Lit("C:\\test\\a.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("a.dll"), String8_Lit("C:\\test\\a.dll"), 0x1000, 0x40);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("b.dll"), String8_Lit("C:\\test\\b.dll"), 0x1040, 0x40);
    Test_MemmyBackend_AddRegion(&backend, 1234, 0x1000, 0x100, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);
    MemmyEval_Value matches = {0};
    Test_EvalExprText(env, arena, "[<a.dll>..<b.dll>] as u8 == 42", &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 1);
    AssertEq(matches.addresses[0], 0x102a);
    AssertEq(backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalRangeEndpointUsesAmbientProcess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Test_MemmyBackend_AddProcess(&backend, 1234, String8_Lit("a.exe"), String8_Lit("C:\\test\\a.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 1234, String8_Lit("b.dll"), String8_Lit("C:\\test\\b.dll"), 0x1040, 0x40);
    Test_MemmyBackend_AddRegion(&backend, 1234, 0x1000, 0x100, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 1234, Memmy_PointerWidth_64);
    MemmyEval_Value matches = {0};
    Test_EvalExprText(env, arena, "[@0x1000..<b.dll>] as u8 == 42", &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 1);
    AssertEq(matches.addresses[0], 0x102a);
    AssertEq(backend.last_open_pid, 1234);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalTransientProcessOpenBookkeepingUsesStatementScratch)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyBackend_Init(&backend);
    Memmy_Backend *memmy_backend = Test_MemmyBackend_AsBackend(&backend);
    Memmy_Context ctx = {.backend = memmy_backend};
    Memmy_Context_Set(&ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    MemmyAst_Statement statement = {0};
    Test_EvalParseStatement(arena, "<test-module.exe>", &statement);
    U64 env_pos = Arena_Pos(arena);
    for (U64 i = 0; i < 3; i++)
    {
        Test_EvalResultCapture capture = {0};
        MemmyEval_ResultSink sink = {
            .callback = Test_EvalResultCapture_Push,
            .user_data = &capture,
        };
        AssertEq(MemmyEval_Statement_Eval(env, &statement, &sink, 0), Memmy_Status_Ok);
        AssertEq(capture.count, 1);
        AssertEq(capture.value.kind, MemmyEval_ValueKind_Range);
        AssertEq(Arena_Pos(arena), env_pos);
    }
    AssertEq(backend.open_call_count, 3);
    AssertEq(backend.close_call_count, 3);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalMissingProcessDiagnostics)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);

    MemmyAst_Node *target = 0;
    Test_EvalParseExpr(arena, "<client.dll>", &target);
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};
    AssertEq(MemmyEval_Expr_Eval(env, target, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("target"));

    MemmyAst_Node *deref = 0;
    Test_EvalParseExpr(arena, "@0x1000->", &deref);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, deref, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("address"));

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalReferenceScanRejectsMissingProcessWrongLhsAndTarget)
{
    Arena *arena = Arena_CreateDefault();
    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyAst_Node *expr = 0;
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};

    Test_EvalParseExpr(arena, "[@0x1000..+0x20] refs ptr @0x1040", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("scan"));

    Test_MemmyBackend backend = {0};
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_EvalParseExpr(arena, "@0x1000 refs ptr @0x1040", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.message, String8_Lit("expected scan range"));

    Test_EvalParseExpr(arena, "[@0x1000..+0x20] refs ptr $not_address", &expr);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("not_address"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_Const, .constant = 1}),
             Memmy_Status_Ok);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("address"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalFunctionLookup)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_MemmyBackend_AddFunction(&backend, 4242, 0x1000, 0x1050);
    Test_MemmyBackend_AddFunction(&backend, 4242, 0x2000, 0x2080);
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"), 0x3000,
                                0x1000);
    Test_MemmyBackend_AddFunction(&backend, 4242, 0x3000, 0x3080);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "function @0x1024", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Range);
    AssertEq(value.range.start, 0x1000);
    AssertEq(value.range.end, 0x1050);

    MemmyEval_Value stored = {0};
    Test_EvalStatementResult(env, arena, "$fn = function @0x1024", &stored);
    AssertEq(stored.kind, MemmyEval_ValueKind_Range);
    AssertEq(stored.range.start, 0x1000);
    AssertEq(stored.range.end, 0x1050);

    Memmy_Addr xrefs[] = {0x1024, 0x2040};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("xrefs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = xrefs,
                                                 .address_count = ArrayCount(xrefs)}),
             Memmy_Status_Ok);
    MemmyEval_Value ranges = {0};
    Test_EvalExprText(env, arena, "$xrefs => function $", &ranges);
    AssertEq(ranges.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(ranges.range_count, 2);
    AssertEq(ranges.ranges[0].start, 0x1000);
    AssertEq(ranges.ranges[0].end, 0x1050);
    AssertEq(ranges.ranges[1].start, 0x2000);
    AssertEq(ranges.ranges[1].end, 0x2080);

    MemmyEval_Value indexed_function = {0};
    Test_EvalExprText(env, arena, "function $xrefs[0]", &indexed_function);
    AssertEq(indexed_function.kind, MemmyEval_ValueKind_Range);
    AssertEq(indexed_function.range.start, 0x1000);
    AssertEq(indexed_function.range.end, 0x1050);

    MemmyEval_Value module_offset = {0};
    Test_EvalExprText(env, arena, "function (<client.dll>+0x24)", &module_offset);
    AssertEq(module_offset.kind, MemmyEval_ValueKind_Range);
    AssertEq(module_offset.range.start, 0x3000);
    AssertEq(module_offset.range.end, 0x3080);

    AssertEq(MemmyEval_Env_Set(env, String8_Lit("hit"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_Address, .address = 0x3024}),
             Memmy_Status_Ok);
    MemmyEval_Value variable_offset = {0};
    Test_EvalExprText(env, arena, "function ($hit + 4)", &variable_offset);
    AssertEq(variable_offset.kind, MemmyEval_ValueKind_Range);
    AssertEq(variable_offset.range.start, 0x3000);
    AssertEq(variable_offset.range.end, 0x3080);

    MemmyEval_Value function_rva = {0};
    Test_EvalExprText(env, arena, "function $hit - <client.dll>", &function_rva);
    AssertEq(function_rva.kind, MemmyEval_ValueKind_Const);
    AssertEq(function_rva.constant, 0);

    MemmyEval_Value module_offset_rva = {0};
    Test_EvalExprText(env, arena, "function (<client.dll>+0x24) - <client.dll>", &module_offset_rva);
    AssertEq(module_offset_rva.kind, MemmyEval_ValueKind_Const);
    AssertEq(module_offset_rva.constant, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalFunctionLookupErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_MemmyBackend_AddFunction(&backend, 4242, 0x1000, 0x1050);

    MemmyAst_Node *expr = 0;
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};

    Test_EvalParseExpr(arena, "function @0x2000", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("function"));

    Memmy_Addr xrefs[] = {0x1024, 0x2000};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("xrefs"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = xrefs,
                                                 .address_count = ArrayCount(xrefs)}),
             Memmy_Status_Ok);
    Test_EvalParseExpr(arena, "$xrefs => function $", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_RangeList);
    AssertEq(value.range_count, 1);
    AssertEq(value.ranges[0].start, 0x1000);
    AssertEq(value.ranges[0].end, 0x1050);

    Test_EvalParseExpr(arena, "function [@0x1000..@0x1050]", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Ok);
    AssertEq(value.kind, MemmyEval_ValueKind_Range);
    AssertEq(value.range.start, 0x1000);
    AssertEq(value.range.end, 0x1050);

    Test_EvalParseExpr(arena, "function $bad", &expr);
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("bad"), (MemmyEval_Value){.kind = MemmyEval_ValueKind_RangeList}),
             Memmy_Status_Ok);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("address"));

    MemmyEval_Env_ClearDefaultProcess(env);
    Test_EvalParseExpr(arena, "function @0x1024", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("function"));

    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    backend.backend.find_function = 0;
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_Unsupported);
    AssertStrEq(error.context, String8_Lit("backend"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalObjectBaseLookup)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    Memmy_Addr object = backend.memory_base + 0x20;
    Test_EvalConfigureObjectBase(&backend, 4242, object, backend.memory_base + 0x80, backend.memory_base + 0xc0);

    MemmyEval_Value value = {0};
    Test_EvalExprText(env, arena, "objectbase @0x1038", &value);
    AssertEq(value.kind, MemmyEval_ValueKind_Address);
    AssertEq(value.address, object);

    MemmyEval_Value stored = {0};
    Test_EvalStatementResult(env, arena, "$obj = objectbase @0x1038", &stored);
    AssertEq(stored.kind, MemmyEval_ValueKind_Address);
    AssertEq(stored.address, object);

    Memmy_Addr hits[] = {0x1038, 0x1038};
    AssertEq(MemmyEval_Env_Set(env, String8_Lit("hits"),
                               (MemmyEval_Value){.kind = MemmyEval_ValueKind_AddressList,
                                                 .addresses = hits,
                                                 .address_count = ArrayCount(hits)}),
             Memmy_Status_Ok);
    MemmyEval_Value bases = {0};
    Test_EvalExprText(env, arena, "$hits => objectbase $", &bases);
    AssertEq(bases.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(bases.address_count, 2);
    AssertEq(bases.addresses[0], object);
    AssertEq(bases.addresses[1], object);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalObjectBaseLookupErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);

    MemmyAst_Node *expr = 0;
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};

    Test_EvalParseExpr(arena, "objectbase @0x1040", &expr);
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_NotFound);
    AssertStrEq(error.context, String8_Lit("objectbase"));

    MemmyEval_Env_ClearDefaultProcess(env);
    Test_EvalParseExpr(arena, "objectbase @0x1038", &expr);
    error = (Memmy_Error){0};
    AssertEq(MemmyEval_Expr_Eval(env, expr, &value, &error), Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("objectbase"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_process_target = TestSuite_Make(
    "Memmy Eval Process And Target", TestCase_Make(Test_MemmyEvalModuleTargetAndProcessRangeResolve),
    TestCase_Make(Test_MemmyEvalModuleTargetVariableStoresPlainAddress),
    TestCase_Make(Test_MemmyEvalModuleAddressArithmetic),
    TestCase_Make(Test_MemmyEvalStoredAddressReadUsesCurrentSelectedProcess),
    TestCase_Make(Test_MemmyEvalAddressFromTypedValueUsesAmbientProcess),
    TestCase_Make(Test_MemmyEvalRangeEndpointsDoNotCarryProcessProvenance),
    TestCase_Make(Test_MemmyEvalRangeEndpointUsesAmbientProcess),
    TestCase_Make(Test_MemmyEvalTransientProcessOpenBookkeepingUsesStatementScratch),
    TestCase_Make(Test_MemmyEvalMissingProcessDiagnostics),
    TestCase_Make(Test_MemmyEvalReferenceScanRejectsMissingProcessWrongLhsAndTarget),
    TestCase_Make(Test_MemmyEvalFunctionLookup), TestCase_Make(Test_MemmyEvalFunctionLookupErrors),
    TestCase_Make(Test_MemmyEvalObjectBaseLookup), TestCase_Make(Test_MemmyEvalObjectBaseLookupErrors), );
