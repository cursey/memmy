#include "test_memmy_common.h"

Test(Test_MemmyCliReplLineEvaluatesExpression)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Cli_RunReplLine(arena, String8_Lit("  <4242!test-module.exe>  \n"), &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000010000000\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplStringEvaluatesLinesAsAscii)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    AssertEq(Memmy_Cli_RunReplString(arena,
                                     String8_Lit("<4242!client.dll>\n"
                                                 "<4242!client.dll>+0x20 : u8\n"
                                                 "exit\n"
                                                 "<4242!client.dll>+0x1\n"),
                                     &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001000\n"
                                 "0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplStringFormatsErrorsAndContinues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Cli_RunReplString(arena, String8_Lit("0x\n<4242!client.dll>\n"), &out, &error),
             Memmy_Status_ParseError);
    AssertStrEq(out, String8_Lit("memmy: parse_error: expected hexadecimal digit\n"
                                 "0x0000000000001000\n"));
    AssertStrEq(error.context, String8_Lit("expr"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionKeepsAssignmentsAcrossLines)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    test_backend.memory[0x20] = 77;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("$addr = <4242!client.dll>+0x20\n"), &out,
                                          &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(should_exit, 0);

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("$addr : u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 77  0x4d\n"));

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("quit\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(should_exit, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_repl =
    TestSuite_Make("Memmy CLI REPL", TestCase_Make(Test_MemmyCliReplLineEvaluatesExpression),
                   TestCase_Make(Test_MemmyCliReplStringEvaluatesLinesAsAscii),
                   TestCase_Make(Test_MemmyCliReplStringFormatsErrorsAndContinues),
                   TestCase_Make(Test_MemmyCliReplSessionKeepsAssignmentsAcrossLines), );
