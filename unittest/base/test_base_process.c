// ===========================================================================
// Process tests
// ===========================================================================

#include "base.h"
#include "test_framework.h"

#if OS_WINDOWS
#define EXE_ECHO "cmd.exe"
#define EXE_ECHO_ARG0 "/c"
#define EXE_ECHO_ARG1 "echo"
#define EXE_SH "cmd.exe"
#define EXE_SH_ARG0 "/c"
#define EXE_CAT "findstr.exe"
#define EXE_CAT_ARG0 "x*"
// findstr emits CRLF-terminated lines; trim trailing whitespace in assertions.
#else
#define EXE_ECHO "/bin/echo"
#define EXE_SH "/bin/sh"
#define EXE_SH_ARG0 "-c"
#define EXE_CAT "/bin/cat"
#endif

Test(Test_ProcessEchoStdout)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8List argv = {0};
    String8List_Push(a, &argv, String8_Lit(EXE_ECHO));
#if OS_WINDOWS
    String8List_Push(a, &argv, String8_Lit(EXE_ECHO_ARG0));
    String8List_Push(a, &argv, String8_Lit(EXE_ECHO_ARG1));
#endif
    String8List_Push(a, &argv, String8_Lit("hi"));
    Process_Result r = Process_Run(a, argv, (String8){0});
    AssertTrue(!r.spawn_failed);
    AssertEq(r.exit_code, 0);
    AssertStrEq(String8_TrimWhitespace(r.stdout_bytes), String8_Lit("hi"));
    Scratch_End(scratch);
}

Test(Test_ProcessNonZeroExit)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8List argv = {0};
    String8List_Push(a, &argv, String8_Lit(EXE_SH));
    String8List_Push(a, &argv, String8_Lit(EXE_SH_ARG0));
    String8List_Push(a, &argv, String8_Lit("exit 7"));
    Process_Result r = Process_Run(a, argv, (String8){0});
    AssertTrue(!r.spawn_failed);
    AssertEq(r.exit_code, 7);
    Scratch_End(scratch);
}

Test(Test_ProcessStdinPipe)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    String8List argv = {0};
    String8List_Push(a, &argv, String8_Lit(EXE_CAT));
#if OS_WINDOWS
    String8List_Push(a, &argv, String8_Lit(EXE_CAT_ARG0));
#endif
    Process_Result r = Process_Run(a, argv, String8_Lit("round trip"));
    AssertTrue(!r.spawn_failed);
    AssertEq(r.exit_code, 0);
    AssertStrEq(String8_TrimWhitespace(r.stdout_bytes), String8_Lit("round trip"));
    Scratch_End(scratch);
}

#if OS_LINUX || OS_MACOS

Test(Test_ProcessPipeline)
{
    Scratch scratch = Scratch_Begin(0, 0);
    Arena *a = scratch.arena;
    Process_PipelineStage stages[2] = {0};
    String8List_Push(a, &stages[0].argv, String8_Lit("/bin/echo"));
    String8List_Push(a, &stages[0].argv, String8_Lit("three line\nblock\nhere"));
    String8List_Push(a, &stages[1].argv, String8_Lit("/usr/bin/wc"));
    String8List_Push(a, &stages[1].argv, String8_Lit("-l"));
    Process_Result r = Process_RunPipeline(a, stages, 2, (String8){0});
    AssertTrue(!r.spawn_failed);
    AssertEq(r.exit_code, 0);
    // echo takes the single arg literally (including embedded LFs) and appends \n;
    // three embedded LFs produce 3 newlines in the stream.
    AssertEq(String8_ToI64(String8_TrimWhitespace(r.stdout_bytes), 10), 3);
    Scratch_End(scratch);
}

TestSuite suite_process =
    TestSuite_Make("Process", TestCase_Make(Test_ProcessEchoStdout), TestCase_Make(Test_ProcessNonZeroExit),
                   TestCase_Make(Test_ProcessStdinPipe), TestCase_Make(Test_ProcessPipeline), );

#else

// Pipeline test omitted on Windows: equivalent `find /c /v ""` decorates
// output with header lines, making the counting assertion awkward.
TestSuite suite_process =
    TestSuite_Make("Process", TestCase_Make(Test_ProcessEchoStdout), TestCase_Make(Test_ProcessNonZeroExit),
                   TestCase_Make(Test_ProcessStdinPipe), );

#endif
