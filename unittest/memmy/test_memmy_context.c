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
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error), Memmy_Status_InvalidArgument);
    AssertEq(error.status, Memmy_Status_InvalidArgument);

    Memmy_Context ctx = {0};
    Memmy_Context_Set(&ctx);
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error), Memmy_Status_InvalidArgument);
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
    AssertEq(Memmy_EnumerateProcesses(arena, Test_ProcessInfoSink(&list, arena), &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);

    Memmy_Process process = {.backend = &backend};
    U8 buffer[4] = {0};
    U64 bytes_read = 99;
    AssertEq(Memmy_Process_Read(&process, 0, buffer, sizeof(buffer), &bytes_read, &error), Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);
    AssertEq(bytes_read, 0);

    U64 bytes_written = 99;
    AssertEq(Memmy_Process_Write(&process, 0, buffer, sizeof(buffer), &bytes_written, &error),
             Memmy_Status_Unsupported);
    AssertEq(error.status, Memmy_Status_Unsupported);
    AssertEq(bytes_written, 0);

    Memmy_Process *opened = (Memmy_Process *)1;
    AssertEq(Memmy_Process_Open(arena, 1, &opened, &error), Memmy_Status_Unsupported);
    AssertTrue(opened == 0);

    Memmy_Range function_range = {.start = 1, .end = 2};
    AssertEq(Memmy_Process_FindFunction(arena, &process, 1, &function_range, &error), Memmy_Status_Unsupported);
    AssertEq(function_range.start, 0);
    AssertEq(function_range.end, 0);

    Memmy_ObjectBaseResult object_base = {.address = 1};
    AssertEq(Memmy_Process_FindObjectBase(arena, &process, 1, 0, &object_base, &error), Memmy_Status_Unsupported);
    AssertEq(object_base.address, 0);

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
