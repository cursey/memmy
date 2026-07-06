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

    AssertEq(Memmy_Cli_RunReplString(arena, String8_Lit("/attach 4242\n<test-module.exe>+0\n"), &out, &error),
             Memmy_Status_Ok);
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
                                     String8_Lit("/attach 4242\n"
                                                 "<client.dll>+0\n"
                                                 "<client.dll>+0x20 as u8\n"
                                                 "/exit\n"
                                                 "<client.dll>+0x1\n"),
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

    AssertEq(Memmy_Cli_RunReplString(arena, String8_Lit("0x\n/attach 4242\n<client.dll>+0\n"), &out, &error),
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

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("$addr = <client.dll>+0x20\n"), &out,
                                          &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(should_exit, 0);

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("$addr as u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 77  0x4d\n"));

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/quit\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(should_exit, 1);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionUsesAttachedProcessForModuleTarget)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("> "));
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001000\n"));
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[test-process:4242]> "));

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("@0x1020 as u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionUsesAttachedProcessRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("> "));
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("[0..]\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[0..]\n"));
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[test-process:4242]> "));

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("@0x1020 as u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionCanSwitchAttachedProcess)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_info_strings_use_enum_arena = 1;
    Test_MemmyBackend_AddProcess(&test_backend, 5678, String8_Lit("other.exe"), String8_Lit("C:\\other\\other.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    Test_MemmyBackend_AddModule(&test_backend, 5678, String8_Lit("client.dll"), String8_Lit("C:\\other\\client.dll"),
                                0x1000, 0x1000);
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001000\n"));
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[test-process:4242]> "));

    error = (Memmy_Error){0};
    test_backend.read_call_count = 0;
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 5678\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0x20 as u8\n"), &out, &should_exit,
                                          &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));
    AssertEq(should_exit, 0);
    AssertEq(test_backend.read_call_count, 1);
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[other.exe:5678]> "));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0x20 as u8\n"), &out, &should_exit,
                                          &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[test-process:4242]> "));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionPidAttachDoesNotRequireProcessEnumeration)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    test_backend.backend.enumerate_processes = 0;
    test_backend.memory[0x20] = 42;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001000\n"));
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[4242]> "));

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("@0x1020 as u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliReplSessionClosesProcessAfterEachStatement)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddModule(&test_backend, 4242, String8_Lit("client.dll"), String8_Lit("C:\\test\\client.dll"),
                                0x1000, 0x1000);
    test_backend.memory[0x20] = 42;
    test_backend.memory[0x21] = 77;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    Memmy_CliReplSession session = Memmy_CliReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};
    B32 should_exit = 0;

    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("/attach 4242\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("<client.dll>+0x20 as u8\n"), &out, &should_exit,
                                          &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001020: u8 42  0x2a\n"));
    AssertEq(test_backend.open_call_count, 1);
    AssertEq(test_backend.close_call_count, 1);
    AssertEq(test_backend.last_close_pid, 4242);
    AssertTrue(session.env->has_default_process);
    AssertEq(session.env->default_pid, 4242);
    AssertStrEq(Memmy_CliReplSession_Prompt(arena, &session), String8_Lit("[test-process:4242]> "));

    error = (Memmy_Error){0};
    AssertEq(Memmy_Cli_RunReplSessionLine(arena, &session, String8_Lit("@0x1021 as u8\n"), &out, &should_exit, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001021: u8 77  0x4d\n"));
    AssertEq(test_backend.open_call_count, 2);
    AssertEq(test_backend.close_call_count, 2);
    AssertTrue(session.env->has_default_process);
    AssertEq(session.env->default_pid, 4242);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_repl =
    TestSuite_Make("Memmy CLI REPL", TestCase_Make(Test_MemmyCliReplLineEvaluatesExpression),
                   TestCase_Make(Test_MemmyCliReplStringEvaluatesLinesAsAscii),
                   TestCase_Make(Test_MemmyCliReplStringFormatsErrorsAndContinues),
                   TestCase_Make(Test_MemmyCliReplSessionKeepsAssignmentsAcrossLines),
                   TestCase_Make(Test_MemmyCliReplSessionUsesAttachedProcessForModuleTarget),
                   TestCase_Make(Test_MemmyCliReplSessionUsesAttachedProcessRange),
                   TestCase_Make(Test_MemmyCliReplSessionCanSwitchAttachedProcess),
                   TestCase_Make(Test_MemmyCliReplSessionPidAttachDoesNotRequireProcessEnumeration),
                   TestCase_Make(Test_MemmyCliReplSessionClosesProcessAfterEachStatement), );
