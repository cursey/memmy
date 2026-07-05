#include "test_memmy_common.h"

Test(Test_MemmyContextSetPushPop)
{
    Memmy_Context ctx_a = {0};
    Memmy_Context ctx_b = {0};

    Memmy_Context_Set(&ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_a);

    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx_b);
    AssertTrue(old_ctx == &ctx_a);
    AssertTrue(Memmy_Context_Get() == &ctx_b);

    Memmy_Context_Pop(old_ctx);
    AssertTrue(Memmy_Context_Get() == &ctx_a);
    Memmy_Context_Set(0);
}

Test(Test_MemmyDispatchRejectsMissingContextAndBackend)
{
    Arena *arena = Arena_CreateDefault();
    Test_ProcessInfoList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(0);
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context ctx = {0};
    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error),
             Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyDispatchRejectsMissingCallback)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Backend backend = {.name = String8_Lit("empty")};
    Memmy_Context ctx = {.backend = &backend};
    Test_ProcessInfoList list = {0};
    Memmy_Error error = {0};

    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Process process = {.backend = &backend};
    U8 buffer[4] = {0};
    U64 bytes_read = 0;
    AssertEq(Memmy_Process_Read(&process, 0, buffer, sizeof(buffer), &bytes_read, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    U64 bytes_written = 0;
    AssertEq(Memmy_Process_Write(&process, 0, buffer, sizeof(buffer), &bytes_written, &error),
             Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCloseMarksProcessClosedWithoutCallback)
{
    Memmy_Backend backend = {.name = String8_Lit("no-close")};
    Memmy_Process process = {.backend = &backend};

    AssertTrue(Memmy_Process_IsOpen(&process));
    Memmy_Process_Close(&process);
    AssertTrue(!Memmy_Process_IsOpen(&process));
    Memmy_Process_Close(&process);
    AssertTrue(!Memmy_Process_IsOpen(&process));
}
TestSuite suite_memmy_context = TestSuite_Make("Memmy Context", TestCase_Make(Test_MemmyContextSetPushPop),
                                               TestCase_Make(Test_MemmyDispatchRejectsMissingContextAndBackend),
                                               TestCase_Make(Test_MemmyDispatchRejectsMissingCallback),
                                               TestCase_Make(Test_MemmyCloseMarksProcessClosedWithoutCallback), );
