#include "test_memmy_eval_common.h"

Test(Test_MemmyEvalCommandsListProcessesModulesAndRegionsWithFuzzyFilters)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Memmy_EvalEnv *env = 0;
    Test_EvalEnvWithProcess(arena, &backend, &env);
    Test_MemmyBackend_AddProcess(&backend, 7777, String8_Lit("ClientGame.exe"), String8_Lit("C:\\game\\ClientGame.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("client-main.dll"),
                                String8_Lit("C:\\test\\client-main.dll"), 0x20000000, 0x3000);
    Test_MemmyBackend_AddModule(&backend, 4242, String8_Lit("physics.dll"), String8_Lit("C:\\test\\physics.dll"),
                                0x30000000, 0x1000);
    Test_MemmyBackend_AddRegion(&backend, 4242, 0x2000, 0x80, Memmy_RegionAccess_Read, Memmy_RegionState_Committed);

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/procs CGe", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Process);
    AssertEq(capture.results[0].process.pid, 7777);

    Test_EvalParseStatement(arena, "/mods cmain", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Module);
    AssertStrEq(capture.results[0].module.name, String8_Lit("client-main.dll"));

    Test_EvalParseStatement(arena, "/regions", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 2);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Region);
    AssertEq(capture.results[1].kind, Memmy_EvalResultKind_Region);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalVarsUnsetAndClearCommands)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Test_EvalStatementText(env, arena, "$foo = 42");
    Test_EvalStatementText(env, arena, "$bar = @0x1000");

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/vars", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 2);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Variable);
    AssertEq(capture.results[1].kind, Memmy_EvalResultKind_Variable);

    Test_EvalParseStatement(arena, "/unset $foo", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Unset);
    AssertStrEq(capture.results[0].name, String8_Lit("foo"));
    Memmy_EvalValue value = {0};
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("foo"), &value), Memmy_Status_NotFound);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("bar"), &value), Memmy_Status_Ok);

    Test_EvalParseStatement(arena, "/clear", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Clear);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("bar"), &value), Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

Test(Test_MemmyEvalHelpAndExitCommandsEmitControlResults)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);

    Memmy_AstStatement statement = {0};
    Test_EvalParseStatement(arena, "/help", &statement);
    Test_EvalResultCapture capture = {0};
    Memmy_EvalResultSink sink = {.push = Test_EvalResultCapture_Push, .user_data = &capture};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Help);
    AssertTrue(String8_Find(capture.results[0].text, String8_Lit("/mods [filter]"), 0) != STRING8_NPOS);

    Test_EvalParseStatement(arena, "/exit", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Exit);

    Test_EvalParseStatement(arena, "/quit", &statement);
    capture = (Test_EvalResultCapture){0};
    AssertEq(Memmy_EvalStatement(env, &statement, &sink, 0), Memmy_Status_Ok);
    AssertEq(capture.count, 1);
    AssertEq(capture.results[0].kind, Memmy_EvalResultKind_Exit);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_command = TestSuite_Make(
    "Memmy Eval Command", TestCase_Make(Test_MemmyEvalCommandsListProcessesModulesAndRegionsWithFuzzyFilters),
    TestCase_Make(Test_MemmyEvalVarsUnsetAndClearCommands),
    TestCase_Make(Test_MemmyEvalHelpAndExitCommandsEmitControlResults), );
