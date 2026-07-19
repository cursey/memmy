#include "test_memmy_common.h"

static Memmy_Status Test_MemmyCliTutorial_Run(Arena *arena, MemmyCli_ReplSession *session, String8 line, String8 *out,
                                              Memmy_Error *error)
{
    B32 should_exit = 0;
    *error = (Memmy_Error){0};
    return MemmyCli_ReplSession_RunLine(arena, session, line, out, &should_exit, error);
}

static Memmy_Addr Test_MemmyCliTutorial_FixtureAddress(String8 instruction)
{
    String8 prefix = String8_Lit("[@0x");
    U64 prefix_pos = String8_Find(instruction, prefix, 0);
    if (prefix_pos == STRING8_NPOS)
    {
        return 0;
    }

    U64 start = prefix_pos + 2;
    U64 end = String8_Find(instruction, String8_Lit(".."), start);
    if (end == STRING8_NPOS)
    {
        return 0;
    }

    Memmy_Addr address = 0;
    Memmy_Address_Parse(String8_Substr(instruction, start, end - start), &address, 0);
    return address;
}

static void Test_MemmyCliTutorial_SeedFixture(Test_MemmyBackend *backend, Memmy_Addr fixture_address)
{
    Test_MemmyBackend_SetMemoryBase(backend, fixture_address);
    for (U64 i = 0; i < 64; i++)
    {
        backend->memory[i] = (U8)(11u + i * 37u);
    }
    backend->memory[8] = 0x78;
    backend->memory[9] = 0x56;
    backend->memory[10] = 0x34;
    backend->memory[11] = 0x12;
    U8 marker[] = {0xde, 0xad, 0xbe, 0xef, 0x13, 0x37, 0xc0, 0xde};
    Memory_Copy(backend->memory + 32, marker, sizeof(marker));
}

Test(Test_MemmyCliTutorialLifecycleControls)
{
    Arena *arena = Arena_CreateDefault();
    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial stop"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("not active"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial hint"), &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("tutorial"));

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial what"), &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("tutorial"));
    AssertStrEq(error.input, String8_Lit("what"));

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial restart"), &out, &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial restarted"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 1/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 1/9"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Welcome"), 0) == STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial hint"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Hint:"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 1/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial stop"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial stopped"), 0) != STRING8_NPOS);

    Arena_Destroy(arena);
}

Test(Test_MemmyCliTutorialRejectsJsonlWithoutBreakingFraming)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl"};

    AssertEq(MemmyCli_Input_RunString(arena, (I32)ArrayCount(argv), argv,
                                      String8_Lit("/tutorial\n"
                                                  "42\n"
                                                  "/tutorial stop\n"),
                                      &out, &error),
             Memmy_Status_InvalidArgument);
    AssertStrEq(error.context, String8_Lit("tutorial"));
    AssertStrEq(error.message, String8_Lit("tutorial is not available with --jsonl"));
    AssertStrEq(error.input, String8_Lit("/tutorial"));
    AssertStrEq(out, String8_Lit("{\"type\":\"error\",\"status\":\"invalid_argument\",\"message\":\"tutorial is not "
                                 "available with --jsonl\",\"context\":\"tutorial\",\"input\":\"/tutorial\","
                                 "\"byte_offset\":0,\"byte_count\":0,\"os_code\":0}\n"
                                 "{\"type\":\"value\",\"kind\":\"const\",\"value_kind\":\"const\",\"value\":42}\n"
                                 "{\"type\":\"error\",\"status\":\"invalid_argument\",\"message\":\"tutorial is not "
                                 "available with --jsonl\",\"context\":\"tutorial\",\"input\":\"/tutorial stop\","
                                 "\"byte_offset\":0,\"byte_count\":0,\"os_code\":0}\n"));

    Arena_Destroy(arena);
}

Test(Test_MemmyCliTutorialSemanticTranscriptAndRecovery)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U32 self_pid = Os_GetProcessId();
    Test_MemmyBackend_AddProcess(&test_backend, self_pid, String8_Lit("memmy-test"),
                                 String8_Lit("C:\\test\\memmy_test.exe"), Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    String8 out = {0};
    Memmy_Error error = {0};

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_PushF(arena, "/attach %u", self_pid), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$early = 84 / 2"), &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 1/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/attach 4242"), &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit(""));
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_PushF(arena, "/attach %u", self_pid), &out, &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 2/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/detach"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Returning to the attach lesson"), 0) != STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_PushF(arena, "/attach %u", self_pid), &out, &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 2/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/mods"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 3/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$forty_two = 84 / 2"), &out, &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 4/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/vars"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("forty_two const\n\nStep complete"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 5/9"), 0) != STRING8_NPOS);
    Memmy_Addr fixture = Test_MemmyCliTutorial_FixtureAddress(out);
    AssertTrue(fixture != 0);
    Test_MemmyCliTutorial_SeedFixture(&test_backend, fixture);
    Test_MemmyBackend_AddRegion(&test_backend, self_pid, fixture, 64, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    U8 fixture_snapshot[64] = {0};
    Memory_Copy(fixture_snapshot, test_backend.memory, sizeof(fixture_snapshot));

    String8 wrong_range = String8_PushF(arena, "[@0x%llx..+0x20]", (unsigned long long)fixture);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, wrong_range, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 6/9"), 0) == STRING8_NPOS);

    String8 exact_range =
        String8_PushF(arena, "[@0x%llx..@0x%llx]", (unsigned long long)fixture, (unsigned long long)(fixture + 64));
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, exact_range, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 6/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session,
                                       String8_PushF(arena, "@0x%llx as u16", (unsigned long long)(fixture + 8)), &out,
                                       &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 7/9"), 0) == STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session,
                                       String8_PushF(arena, "@0x%llx as u32", (unsigned long long)(fixture + 8)), &out,
                                       &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("0x12345678"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("\n\nStep complete"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 7/9"), 0) != STRING8_NPOS);

    String8 wrong_scan =
        String8_PushF(arena, "$wrong = [@0x%llx..+0x40]{0b 30 55 7a 9f c4 e9 0e}", (unsigned long long)fixture);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, wrong_scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) == STRING8_NPOS);

    String8 scan =
        String8_PushF(arena, "$hits = [@0x%llx..+0x40]{de ad be ef 13 37 c0 de}", (unsigned long long)fixture);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$hits |> $[0]"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial complete"), 0) == STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$hits = 42"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("cleared or changed"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Returning to the scan lesson"), 0) != STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/unset $hits"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Returning to the scan lesson"), 0) != STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$hits[1 - 1]"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 9/9"), 0) != STRING8_NPOS);

    String8 unrelated_scan =
        String8_PushF(arena, "$hits = [@0x%llx..+0x40]{0b 30 55 7a 9f c4 e9 0e}", (unsigned long long)fixture);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, unrelated_scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("cleared or changed"), 0) != STRING8_NPOS);
    AssertTrue(String8_Find(out, String8_Lit("Returning to the scan lesson"), 0) != STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, scan, &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 8/9"), 0) != STRING8_NPOS);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$hits[0]"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial 9/9"), 0) != STRING8_NPOS);

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$hits |> $[0]"), &out, &error), Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial complete"), 0) != STRING8_NPOS);
    AssertTrue(Memory_Equals(fixture_snapshot, test_backend.memory, sizeof(fixture_snapshot)));

    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial hint"), &out, &error),
             Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliTutorialDefaultBackendSmokeTranscript)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Context ctx = {0};
    Memmy_Error error = {0};
    Memmy_Status status = Memmy_Context_InitDefault(arena, &ctx, &error);
#if OS_WINDOWS || OS_MACOS
    AssertEq(status, Memmy_Status_Ok);
    Memmy_Context *old_ctx = Memmy_Context_Push(&ctx);

    MemmyCli_ReplSession session = MemmyCli_ReplSession_Begin(arena);
    String8 out = {0};
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/tutorial"), &out, &error), Memmy_Status_Ok);
    AssertEq(
        Test_MemmyCliTutorial_Run(arena, &session, String8_PushF(arena, "/attach %u", Os_GetProcessId()), &out, &error),
        Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/mods"), &out, &error), Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$answer = 6 * 7"), &out, &error), Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("/vars"), &out, &error), Memmy_Status_Ok);
    Memmy_Addr fixture = Test_MemmyCliTutorial_FixtureAddress(out);
    AssertTrue(fixture != 0);

    AssertEq(Test_MemmyCliTutorial_Run(
                 arena, &session, String8_PushF(arena, "[@0x%llx..+0x40]", (unsigned long long)fixture), &out, &error),
             Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session,
                                       String8_PushF(arena, "@0x%llx as u32", (unsigned long long)(fixture + 8)), &out,
                                       &error),
             Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session,
                                       String8_PushF(arena, "$matches = [@0x%llx..+0x40]{de ad be ef 13 37 c0 de}",
                                                     (unsigned long long)fixture),
                                       &out, &error),
             Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$matches[0]"), &out, &error), Memmy_Status_Ok);
    AssertEq(Test_MemmyCliTutorial_Run(arena, &session, String8_Lit("$matches |> $[0]"), &out, &error),
             Memmy_Status_Ok);
    AssertTrue(String8_Find(out, String8_Lit("Tutorial complete"), 0) != STRING8_NPOS);

    Memmy_Context_Pop(old_ctx);
#else
    AssertEq(status, Memmy_Status_Unsupported);
#endif

    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_tutorial =
    TestSuite_Make("Memmy CLI Tutorial", TestCase_Make(Test_MemmyCliTutorialLifecycleControls),
                   TestCase_Make(Test_MemmyCliTutorialRejectsJsonlWithoutBreakingFraming),
                   TestCase_Make(Test_MemmyCliTutorialSemanticTranscriptAndRecovery),
                   TestCase_Make(Test_MemmyCliTutorialDefaultBackendSmokeTranscript), );
