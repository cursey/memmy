#include "test_memmy_common.h"

Test(Test_MemmyCliPeekTextOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 str_bytes[] = {'h', 'e', 'l', 'l', 'o'};
    U8 wstr_bytes[] = {'H', 0, 'i', 0};
    U8 escaped_str_bytes[] = {'a', 0, 'b', '\n', '"', '\\', 0x7f};
    U8 escaped_wstr_bytes[] = {'A', 0, 0, 0, '\n', 0, 'B', 0};
    memcpy(test_backend.memory + 0x40, str_bytes, sizeof(str_bytes));
    memcpy(test_backend.memory + 0x50, wstr_bytes, sizeof(wstr_bytes));
    memcpy(test_backend.memory + 0x60, escaped_str_bytes, sizeof(escaped_str_bytes));
    memcpy(test_backend.memory + 0x70, escaped_wstr_bytes, sizeof(escaped_wstr_bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *u32_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1002", "--type", "u32"};
    char *ptr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1008", "--type", "ptr"};
    char *bytes_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x100a", "--type", "bytes", "--count", "3"};
    char *str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1040", "--type", "str", "--count", "5"};
    char *wstr_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1050", "--type", "wstr", "--count", "2"};
    char *escaped_str_argv[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1060", "--type", "str", "--count", "7"};
    char *escaped_wstr_argv[] = {"memmy",  "peek",   "--pid", "4242",    "--addr",
                                 "0x1070", "--type", "wstr",  "--count", "4"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(u32_argv), u32_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001002: u32 84148994  0x05040302\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001008: ptr 1084818905618843912  0x0f0e0d0c0b0a0908\n"));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(ptr_argv), ptr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x00001008: ptr 185207048  0x0b0a0908\n"));
    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bytes_argv), bytes_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x000000000000100a: bytes 0a 0b 0c\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(str_argv), str_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001040: str \"hello\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(wstr_argv), wstr_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001050: wstr \"Hi\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_str_argv), escaped_str_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001060: str \"a\\0b\\n\\\"\\\\\\x7f\"\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(escaped_wstr_argv), escaped_wstr_argv, &out, &error),
             Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("0x0000000000001070: wstr \"A\\0\\nB\"\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekCountAndAddressValidation)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *missing_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "bytes"};
    char *extra_count[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000", "--type", "u16", "--count", "2"};
    char *bad_addr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1000+4", "--type", "u8"};
    char *bad_str[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x10ff", "--type", "str", "--count", "1"};
    char *bad_wstr[] = {"memmy", "peek", "--pid", "4242", "--addr", "0x1080", "--type", "wstr", "--count", "1"};

    test_backend.memory[0xff] = 0xff;
    test_backend.memory[0x80] = 0x00;
    test_backend.memory[0x81] = 0xd8;

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(missing_count), missing_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(extra_count), extra_count, &out, &error),
             Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_addr), bad_addr, &out, &error), Memmy_Status_ParseError);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_str), bad_str, &out, &error),
             Memmy_Status_InvalidEncoding);
    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(bad_wstr), bad_wstr, &out, &error),
             Memmy_Status_InvalidEncoding);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekJsonNonFiniteFloatValuesAreValidJson)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    U32 f32_nan_bits = 0x7fc00000;
    U64 f64_inf_bits = 0x7ff0000000000000ull;
    memcpy(test_backend.memory + 0x20, &f32_nan_bits, sizeof(f32_nan_bits));
    memcpy(test_backend.memory + 0x30, &f64_inf_bits, sizeof(f64_inf_bits));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *f32_nan_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1020", "--type", "f32"};
    char *f64_inf_argv[] = {"memmy", "--json", "peek", "--pid", "4242", "--addr", "0x1030", "--type", "f64"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f32_nan_argv), f32_nan_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001020\",\"type\":\"f32\",\"value\":null}\n"));

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(f64_inf_argv), f64_inf_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001030\",\"type\":\"f64\",\"value\":null}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyCliPeekJsonSuccessOutput)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.process_count = 0;
    Test_MemmyBackend_AddProcess(&test_backend, 222, String8_Lit("beta.exe"), String8_Lit("C:\\beta.exe"),
                                 Memmy_PointerWidth_64);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);

    String8 out = {0};
    Memmy_Error error = {0};
    char *peek_argv[] = {"memmy", "--json", "peek", "--name", "beta.exe", "--addr", "0x1002", "--type", "u32"};

    AssertEq(Memmy_Cli_RunToString(arena, (I32)ArrayCount(peek_argv), peek_argv, &out, &error), Memmy_Status_Ok);
    AssertStrEq(out, String8_Lit("{\"address\":\"0x0000000000001002\",\"type\":\"u32\",\"value\":84148994,"
                                 "\"hex\":\"0x05040302\"}\n"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_cli_peek = TestSuite_Make("Memmy CLI peek", TestCase_Make(Test_MemmyCliPeekTextOutput),
                                                TestCase_Make(Test_MemmyCliPeekCountAndAddressValidation),
                                                TestCase_Make(Test_MemmyCliPeekJsonNonFiniteFloatValuesAreValidJson),
                                                TestCase_Make(Test_MemmyCliPeekJsonSuccessOutput), );
