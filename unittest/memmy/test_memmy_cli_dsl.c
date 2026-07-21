#include "test_memmy_common.h"

static void Test_MemmyCliSemantic_Setup(Test_MemmyBackend *backend)
{
    Test_MemmyBackend_Init(backend);
    Test_MemmyBackend_AddProcess(backend, 1234, String8_Lit("game.exe"), String8_Lit("C:\\game\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddModule(backend, 1234, String8_Lit("client.dll"), String8_Lit("C:\\game\\client.dll"), 0x1000,
                                0x2000);
    Test_MemmyBackend_AddRegion(backend, 1234, 0x1000, TEST_MEMMY_BACKEND_MEMORY_SIZE,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);
}

Test(Test_MemmyCliFormatsSemanticScalarValuesText)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyCliSemantic_Setup(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    String8 out = {0};
    Memmy_Error error = {0};

    char *integer[] = {"memmy", "--expr", "42"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(integer), integer, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("i64 42\n"));
    char *address[] = {"memmy", "--pid", "1234", "--expr", "<client.dll>+0x20"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(address), address, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("address 0x0000000000001020\n"));
    char *range[] = {"memmy", "--pid", "1234", "--expr", "[<client.dll>..+0x20]"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(range), range, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("range [0x0000000000001000..0x0000000000001020)\n"));
    char *nil[] = {"memmy", "--expr", "nil"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(nil), nil, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("null nil\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliFormatsSemanticScalarValuesJsonl)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyCliSemantic_Setup(&backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    String8 out = {0};
    Memmy_Error error = {0};

    char *integer[] = {"memmy", "--jsonl", "--expr", "42"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(integer), integer, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"value\",\"value_type\":\"i64\",\"value\":42}\n"));
    char *address[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "<client.dll>+0x20"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(address), address, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"value\",\"value_type\":\"address\",\"value\":\"0x0000000000001020\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliFormatsDecodedReadsAsValues)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyCliSemantic_Setup(&backend);
    backend.memory[0x20] = 42;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--pid", "1234", "--expr", "@0x1020 as u8"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(argv), argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("u8 42\n"));
    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliStreamsSemanticAddressLists)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    Test_MemmyCliSemantic_Setup(&backend);
    backend.memory[0x20] = 0xaa;
    backend.memory[0x30] = 0xaa;
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&backend)};
    Memmy_Context_Set(&ctx);
    String8 out = {0};
    Memmy_Error error = {0};

    char *text[] = {"memmy", "--pid", "1234", "--expr", "[@0x1000..+0x40]{aa}"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(text), text, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("[0] address 0x0000000000001020\n"
                                 "[1] address 0x0000000000001030\n"
                                 "list<address> count 2\n"));
    char *json[] = {"memmy", "--jsonl", "--pid", "1234", "--expr", "[@0x1000..+0x40]{aa}"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(json), json, &out, &error), Memmy_Status_Ok);
    AssertStrEq(
        out,
        String8_Lit("{\"type\":\"list_item\",\"value_type\":\"address\",\"index\":0,\"value\":\"0x0000000000001020\"}\n"
                    "{\"type\":\"list_item\",\"value_type\":\"address\",\"index\":1,\"value\":\"0x0000000000001030\"}\n"
                    "{\"type\":\"summary\",\"value_type\":\"list<address>\",\"count\":2}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliVarsReportsExactSemanticTypes)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};
    char *argv[] = {"memmy", "--jsonl"};
    AssertEq(MemmyCli_Input_RunString(arena, ArrayCount(argv), argv, String8_Lit("$n = 42\n/vars\n"), &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"assignment\",\"name\":\"n\",\"value_type\":\"i64\"}\n"
                                 "{\"type\":\"variable\",\"name\":\"n\",\"value_type\":\"i64\"}\n"));
    Arena_Destroy(arena);
}

Test(Test_MemmyCliFormatsFloatAndStringLiterals)
{
    Arena *arena = Arena_CreateDefault();
    String8 out = {0};
    Memmy_Error error = {0};

    char *float_text[] = {"memmy", "--expr", "42.5 as f32"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(float_text), float_text, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("f32 42.5\n"));
    char *string_text[] = {"memmy", "--expr", "\"hello\\nworld\""};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(string_text), string_text, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("str \"hello\\nworld\"\n"));
    char *json[] = {"memmy", "--jsonl", "--expr", "42.5"};
    AssertEq(MemmyCli_Argv_RunToString(arena, ArrayCount(json), json, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"type\":\"value\",\"value_type\":\"f64\",\"value\":42.5}\n"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_dsl = TestSuite_Make(
    "Memmy CLI DSL", TestCase_Make(Test_MemmyCliFormatsSemanticScalarValuesText),
    TestCase_Make(Test_MemmyCliFormatsSemanticScalarValuesJsonl),
    TestCase_Make(Test_MemmyCliFormatsDecodedReadsAsValues), TestCase_Make(Test_MemmyCliStreamsSemanticAddressLists),
    TestCase_Make(Test_MemmyCliVarsReportsExactSemanticTypes),
    TestCase_Make(Test_MemmyCliFormatsFloatAndStringLiterals), );
