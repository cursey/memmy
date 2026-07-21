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

void Test_DisableEnumerateRegions(Test_MemmyBackend *backend)
{
    backend->backend.enumerate_regions = 0;
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

void Test_EncodeValue(Arena *arena, Memmy_Value value, Memmy_EncodedValue *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Value_Encode(arena, &value, out, &error), Memmy_Status_Ok);
}

void Test_OpenProcess(Arena *arena, Memmy_Process **out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Process_Open(arena, 4242, out, &error), Memmy_Status_Ok);
}

Memmy_ScanSink Test_ScanSink(Test_ScanResultList *results, Arena *arena)
{
    *results = (Test_ScanResultList){
        .arena = arena,
        .status = Memmy_Status_Ok,
    };
    Memmy_ScanSink sink = {
        .callback = Test_ScanSinkCallback,
        .user_data = results,
    };
    return sink;
}

Memmy_Status Test_ScanSinkCallback(void *user_data, Memmy_Addr address)
{
    Test_ScanResultList *results = (Test_ScanResultList *)user_data;
    Test_ScanResult *result = Arena_PushStruct(results->arena, Test_ScanResult);
    result->address = address;
    List_PushBack(&results->list, &result->link);
    return results->status;
}

void Test_AssertScanAddresses(Test_ScanResultList *results, Memmy_Addr *addresses, U64 count)
{
    AssertEq(results->list.count, count);
    U64 index = 0;
    List_ForEach(Test_ScanResult, result, &results->list, link)
    {
        AssertTrue(index < count);
        AssertEq(result->address, addresses[index]);
        index++;
    }
}

Memmy_ProcessInfoSink Test_ProcessInfoSink(Test_ProcessInfoList *results, Arena *arena)
{
    *results = (Test_ProcessInfoList){
        .arena = arena,
        .status = Memmy_Status_Ok,
    };
    Memmy_ProcessInfoSink sink = {
        .callback = Test_ProcessInfoSinkCallback,
        .user_data = results,
    };
    return sink;
}

Memmy_Status Test_ProcessInfoSinkCallback(void *user_data, Memmy_ProcessInfo const *info)
{
    Test_ProcessInfoList *results = (Test_ProcessInfoList *)user_data;
    Test_ProcessInfoNode *node = Arena_PushStruct(results->arena, Test_ProcessInfoNode);
    node->info = *info;
    List_PushBack(&results->list, &node->link);
    return results->status;
}

Memmy_ModuleSink Test_ModuleSink(Test_ModuleList *results, Arena *arena)
{
    *results = (Test_ModuleList){
        .arena = arena,
        .status = Memmy_Status_Ok,
    };
    Memmy_ModuleSink sink = {
        .callback = Test_ModuleSinkCallback,
        .user_data = results,
    };
    return sink;
}

Memmy_Status Test_ModuleSinkCallback(void *user_data, Memmy_Module const *module)
{
    Test_ModuleList *results = (Test_ModuleList *)user_data;
    Test_ModuleNode *node = Arena_PushStruct(results->arena, Test_ModuleNode);
    node->module = *module;
    List_PushBack(&results->list, &node->link);
    return results->status;
}

Memmy_RegionSink Test_RegionSink(Test_RegionList *results, Arena *arena)
{
    *results = (Test_RegionList){
        .arena = arena,
        .status = Memmy_Status_Ok,
    };
    Memmy_RegionSink sink = {
        .callback = Test_RegionSinkCallback,
        .user_data = results,
    };
    return sink;
}

Memmy_Status Test_RegionSinkCallback(void *user_data, Memmy_Region const *region)
{
    Test_RegionList *results = (Test_RegionList *)user_data;
    Test_RegionNode *node = Arena_PushStruct(results->arena, Test_RegionNode);
    node->region = *region;
    List_PushBack(&results->list, &node->link);
    return results->status;
}
