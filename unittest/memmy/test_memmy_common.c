#include "test_memmy_common.h"

void Test_AssertBytes(String8 actual, U8 *expected, U64 expected_len)
{
    AssertEq(actual.len, expected_len);
    for (U64 i = 0; i < expected_len; i++)
    {
        AssertEq(actual.data[i], expected[i]);
    }
}

void Test_ParseType(String8 text, Memmy_Type *out)
{
    AssertEq(Memmy_Type_Parse(text, out, 0), Memmy_Status_Ok);
}

void Test_DisableListRegions(Test_MemmyBackend *backend)
{
    backend->backend.list_regions = 0;
}

void Test_ResetOpenTracking(Test_MemmyBackend *backend)
{
    backend->open_call_count = 0;
    backend->last_open_pid = 0;
}

void Test_ParsePattern(Arena *arena, char *text, Memmy_Pattern *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Pattern_Parse(arena, String8_FromCStr(text), Memmy_PatternParseFlag_AllowWildcards, out, &error),
             Memmy_Status_Ok);
}

void Test_ParseValue(Arena *arena, char *type_text, Memmy_PointerWidth pointer_width, char *value_text,
                     Memmy_Value *out)
{
    Memmy_Error error = {0};
    Memmy_Type type = {0};
    AssertEq(Memmy_Type_Parse(String8_FromCStr(type_text), &type, &error), Memmy_Status_Ok);
    AssertEq(Memmy_Value_Parse(arena, type, pointer_width, String8_FromCStr(value_text), out, &error), Memmy_Status_Ok);
}

void Test_OpenProcess(Arena *arena, Memmy_Process **out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, out, &error), Memmy_Status_Ok);
}

void Test_AssertScanAddresses(Memmy_ScanResultList *results, Memmy_Addr *addresses, U64 count)
{
    AssertEq(results->list.count, count);
    U64 index = 0;
    List_ForEach(Memmy_ScanResult, result, &results->list, link)
    {
        AssertTrue(index < count);
        AssertEq(result->address, addresses[index]);
        index++;
    }
}
