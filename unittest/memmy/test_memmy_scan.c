#include "test_memmy_common.h"

static void Test_WriteU32LE(U8 *dst, U32 value)
{
    for (U64 i = 0; i < 4; i++)
    {
        dst[i] = (U8)(value >> (i * 8));
    }
}

static void Test_WriteU64LE(U8 *dst, U64 value)
{
    for (U64 i = 0; i < 8; i++)
    {
        dst[i] = (U8)(value >> (i * 8));
    }
}

static B32 Test_ScanCustom_Match(void *user_data, Memmy_Addr address, U8 const *bytes, U64 available)
{
    Unused(user_data);
    Unused(address);
    return available != 0 && bytes[0] == 0xcc;
}

Test(Test_MemmyScanFindsBeginningMiddleAndEnd)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 pattern_bytes[] = {0xaa, 0xbb, 0xcc};
    memcpy(test_backend.memory + 0, pattern_bytes, sizeof(pattern_bytes));
    memcpy(test_backend.memory + 0x40, pattern_bytes, sizeof(pattern_bytes));
    memcpy(test_backend.memory + 0xfd, pattern_bytes, sizeof(pattern_bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "aa bb cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1100}, .chunk_size = 0x20};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1000, 0x1040, 0x10fd};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyReferenceScanFindsPtr32AndPtr64)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 5};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    Test_WriteU32LE(test_backend.memory + 0x20, 0x11223344);
    Test_OpenProcess(arena, &process);
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x11223344,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected32[] = {0x1020};
    Test_AssertScanAddresses(&results, expected32, ArrayCount(expected32));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;
    Test_WriteU64LE(test_backend.memory + 0x30, 0x1122334455667788);
    Test_OpenProcess(arena, &process);
    options.range.start = 0x1030;
    options.range.end = 0x1040;
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1122334455667788,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected64[] = {0x1030};
    Test_AssertScanAddresses(&results, expected64, ArrayCount(expected64));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyReferenceScanFindsRel32PositiveNegativeAndAnyDeduped)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_WriteU32LE(test_backend.memory + 0x10, 0x20 - 0x10 - 4);
    Test_WriteU32LE(test_backend.memory + 0x30, (U32)(I32)(0x20 - 0x30 - 4));
    Test_WriteU64LE(test_backend.memory + 0x40, 0x1020);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1050}, .chunk_size = 7};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Rel32, 0x1020,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr rel32_expected[] = {0x1010, 0x1030};
    Test_AssertScanAddresses(&results, rel32_expected, ArrayCount(rel32_expected));

    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Any, 0x1020,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr any_expected[] = {0x1010, 0x1030, 0x1040};
    Test_AssertScanAddresses(&results, any_expected, ArrayCount(any_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyReferenceScanChunkLimitRegionsUnreadableAndInvalidArguments)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_WriteU64LE(test_backend.memory + 3, 0x1122334455667788);
    Test_WriteU64LE(test_backend.memory + 0x20, 0x1122334455667788);
    Test_WriteU64LE(test_backend.memory + 0x30, 0x1122334455667788);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1010}, .chunk_size = 9};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1122334455667788,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr boundary_expected[] = {0x1003};
    Test_AssertScanAddresses(&results, boundary_expected, ArrayCount(boundary_expected));

    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .limit = 1, .chunk_size = 9};
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1122334455667788,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr limit_expected[] = {0x1020};
    Test_AssertScanAddresses(&results, limit_expected, ArrayCount(limit_expected));

    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 9};
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1122334455667788,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr region_expected[] = {0x1030};
    Test_AssertScanAddresses(&results, region_expected, ArrayCount(region_expected));

    Test_DisableEnumerateRegions(&test_backend);
    test_backend.unreadable_range_count = 0;
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1020, 0x1040);
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1122334455667788,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_Unknown;
    Test_OpenProcess(arena, &process);
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1000,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, (Memmy_ReferenceScanMode)99, 0x1000,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_InvalidArgument);
    AssertEq(Memmy_Process_ScanReferences(arena, 0, &options, Memmy_ReferenceScanMode_Ptr, 0x1000,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_InvalidArgument);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanSinkReceivesMatchesInAddressOrder)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1010, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x50] = 0xab;
    test_backend.memory[0x10] = 0xab;
    test_backend.memory[0x30] = 0xab;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ab", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1070}, .chunk_size = 8};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1010, 0x1030, 0x1050};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanReusesOneTransientBufferAcrossRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1010, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ff", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1070}, .chunk_size = 8};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};
    U64 arena_pos = Arena_Pos(arena);

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    AssertEq(results.list.count, 0);
    AssertTrue(test_backend.read_call_count > 3);
    AssertTrue(test_backend.first_read_buffer != 0);
    AssertEq(test_backend.read_buffer_changed, 0);
    AssertEq(Arena_Pos(arena), arena_pos);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanPropagatesSinkError)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.memory[0x10] = 0xcd;
    test_backend.memory[0x20] = 0xcd;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "cd", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 8};
    Test_ScanResultList results = {
        .arena = arena,
        .status = Memmy_Status_AccessDenied,
    };
    Memmy_ScanSink sink = {
        .callback = Test_ScanSinkCallback,
        .user_data = &results,
    };
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, sink, &error), Memmy_Status_AccessDenied);
    Memmy_Addr expected[] = {0x1010};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanDoesNotReadOutsideRequestedRangeAndAllowsZeroLength)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "10 11", &pattern);
    Memmy_Error error = {0};
    Test_ScanResultList results = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 7};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    AssertTrue(test_backend.min_read_addr >= options.range.start);
    AssertTrue(test_backend.max_read_end <= options.range.end);

    test_backend.read_call_count = 0;
    Test_ParsePattern(arena, "10 11 12", &pattern);
    options = (Memmy_ScanOptions){.range = {.start = 0x1010, .end = 0x1012}, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    AssertEq(results.list.count, 0);
    AssertTrue(test_backend.read_call_count > 0);

    test_backend.read_call_count = 0;
    options.range.end = options.range.start;
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    AssertEq(results.list.count, 0);
    AssertEq(test_backend.read_call_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanFindsChunkBoundaryMatchesAndHonorsLimit)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 boundary[] = {0xde, 0xad, 0xbe};
    memcpy(test_backend.memory + 3, boundary, sizeof(boundary));
    test_backend.memory[0x20] = 0xfa;
    test_backend.memory[0x22] = 0xfa;
    test_backend.memory[0x24] = 0xfa;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "de ad be", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr boundary_expected[] = {0x1003};
    Test_AssertScanAddresses(&results, boundary_expected, ArrayCount(boundary_expected));

    Test_ParsePattern(arena, "fa", &pattern);
    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1028}, .limit = 2, .chunk_size = 3};
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr limit_expected[] = {0x1020, 0x1022};
    Test_AssertScanAddresses(&results, limit_expected, ArrayCount(limit_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanUsesRegionIntersectionWhenAvailable)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x10] = 0xcc;
    test_backend.memory[0x22] = 0xcc;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 8};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1022};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanFindsPatternAcrossAdjacentReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    U8 bytes[] = {0xde, 0xad, 0xbe, 0xef};
    memcpy(test_backend.memory + 0x2e, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x4e, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "de ad be ef", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 0x10};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x102e};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanRequiresRegionEnumeration)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_DisableEnumerateRegions(&test_backend);
    test_backend.region_count = 0;
    test_backend.memory[0x10] = 0xcc;
    test_backend.memory[0x22] = 0xcc;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "cc", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1030}, .chunk_size = 8};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(results.list.count, 0);

    Memmy_EncodedValue value = {0};
    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_U8, .unsigned_integer = 204}, &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(Memmy_Process_ScanReferences(arena, process, &options, Memmy_ReferenceScanMode_Ptr, 0x1000,
                                          Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(Memmy_Process_ScanCustom(arena, process, &options, 1, 1, Test_ScanCustom_Match, 0,
                                      Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanSkipsUnreadableHolesAndReportsFullyUnreadableRange)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1040, 0x1050);
    test_backend.memory[0x32] = 0xab;
    test_backend.memory[0x52] = 0xab;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ab", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1030, .end = 0x1060}, .chunk_size = 16};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1032, 0x1052};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    test_backend.unreadable_range_count = 0;
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1030, 0x1060);
    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unreadable);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanScansPartialReads)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    Test_MemmyBackend_SetReadLimit(&test_backend, 3);
    U8 bytes[] = {0x80, 0x81};
    memcpy(test_backend.memory + 1, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "80 81", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1001};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyScanSkipsNonReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1010, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Free);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read, Memmy_RegionState_Reserved);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read | Memmy_RegionAccess_Guard,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    test_backend.memory[0x10] = 0xee;
    test_backend.memory[0x20] = 0xee;
    test_backend.memory[0x30] = 0xee;
    test_backend.memory[0x40] = 0xee;
    test_backend.memory[0x50] = 0xee;

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_Pattern pattern = {0};
    Test_ParsePattern(arena, "ee", &pattern);
    Memmy_ScanOptions options = {.range = {.start = 0x1010, .end = 0x1060}, .chunk_size = 8};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanPattern(arena, process, &options, pattern, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1050};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanFindsScalarValuesAtMultipleAlignments)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 scalar[] = {0x34, 0x12};
    memcpy(test_backend.memory + 1, scalar, sizeof(scalar));
    memcpy(test_backend.memory + 4, scalar, sizeof(scalar));
    memcpy(test_backend.memory + 7, scalar, sizeof(scalar));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_EncodedValue value = {0};
    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_U16, .unsigned_integer = 0x1234}, &value);
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x100a}, .chunk_size = 3};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x1001, 0x1004, 0x1007};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanPointerWidthAware)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 ptr32[] = {0x44, 0x33, 0x22, 0x11};
    U8 ptr64[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Memmy_EncodedValue value = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 5};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_32;
    memcpy(test_backend.memory + 0x20, ptr32, sizeof(ptr32));
    Test_OpenProcess(arena, &process);
    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_U32, .unsigned_integer = 0x11223344}, &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected32[] = {0x1020};
    Test_AssertScanAddresses(&results, expected32, ArrayCount(expected32));

    test_backend.processes[0].pointer_width = Memmy_PointerWidth_64;
    memcpy(test_backend.memory + 0x30, ptr64, sizeof(ptr64));
    Test_OpenProcess(arena, &process);
    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_U64, .unsigned_integer = 0x1122334455667788}, &value);
    options.range.start = 0x1030;
    options.range.end = 0x1040;
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected64[] = {0x1030};
    Test_AssertScanAddresses(&results, expected64, ArrayCount(expected64));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanBytesUtf8AndUtf16)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0x48, 0x8b};
    U8 str[] = {'A', 'z', 0};
    U8 wstr[] = {'A', 0, 'z', 0, 0, 0};
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, str, sizeof(str));
    memcpy(test_backend.memory + 0x40, "az", 2);
    memcpy(test_backend.memory + 0x50, wstr, sizeof(wstr));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_EncodedValue value = {0};
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 3};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    value = (Memmy_EncodedValue){.bytes = String8_Make(bytes, sizeof(bytes))};
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr bytes_expected[] = {0x1020};
    Test_AssertScanAddresses(&results, bytes_expected, ArrayCount(bytes_expected));

    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_Str, .string = String8_Lit("Az")}, &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr str_expected[] = {0x1030};
    Test_AssertScanAddresses(&results, str_expected, ArrayCount(str_expected));

    Test_EncodeValue(arena, (Memmy_Value){.type = Memmy_Type_WStr, .string = String8_Lit("Az")}, &value);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr wstr_expected[] = {0x1050};
    Test_AssertScanAddresses(&results, wstr_expected, ArrayCount(wstr_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanRangeChunkLimitRegionAndReadErrors)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    U8 bytes[] = {0xde, 0xad, 0xbe};
    memcpy(test_backend.memory + 3, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x20, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x30, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_EncodedValue value = {.bytes = String8_Make(bytes, sizeof(bytes))};
    Memmy_ScanOptions options = {.range = {.start = 0x1000, .end = 0x1008}, .chunk_size = 4};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr boundary_expected[] = {0x1003};
    Test_AssertScanAddresses(&results, boundary_expected, ArrayCount(boundary_expected));

    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .limit = 1, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr limit_expected[] = {0x1020};
    Test_AssertScanAddresses(&results, limit_expected, ArrayCount(limit_expected));

    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    options = (Memmy_ScanOptions){.range = {.start = 0x1020, .end = 0x1040}, .chunk_size = 8};
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr region_expected[] = {0x1030};
    Test_AssertScanAddresses(&results, region_expected, ArrayCount(region_expected));

    Memmy_Status (*enumerate_regions)(Arena *, Memmy_Process *, Memmy_RegionSink, Memmy_Error *) =
        test_backend.backend.enumerate_regions;
    Test_DisableEnumerateRegions(&test_backend);
    Test_MemmyBackend_AddUnreadableRange(&test_backend, 0x1028, 0x1030);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Unsupported);
    AssertEq(results.list.count, 0);

    test_backend.backend.enumerate_regions = enumerate_regions;
    test_backend.unreadable_range_count = 0;
    Test_MemmyBackend_SetReadLimit(&test_backend, 4);
    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Test_AssertScanAddresses(&results, region_expected, ArrayCount(region_expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyValueScanFindsValueAcrossAdjacentReadableRegions)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend test_backend = {0};
    Test_MemmyBackend_Init(&test_backend);
    test_backend.region_count = 0;
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1020, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1030, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1040, 0x10, 0, Memmy_RegionState_Committed);
    Test_MemmyBackend_AddRegion(&test_backend, 4242, 0x1050, 0x10, Memmy_RegionAccess_Read,
                                Memmy_RegionState_Committed);
    U8 bytes[] = {0xca, 0xfe, 0xba, 0xbe};
    memcpy(test_backend.memory + 0x2e, bytes, sizeof(bytes));
    memcpy(test_backend.memory + 0x4e, bytes, sizeof(bytes));

    Memmy_Context ctx = {.backend = Test_MemmyBackend_AsBackend(&test_backend)};
    Memmy_Context_Set(&ctx);
    Memmy_Process *process = 0;
    Test_OpenProcess(arena, &process);
    Memmy_EncodedValue value = {.bytes = String8_Make(bytes, sizeof(bytes))};
    Memmy_ScanOptions options = {.range = {.start = 0x1020, .end = 0x1060}, .chunk_size = 0x10};
    Test_ScanResultList results = {0};
    Memmy_Error error = {0};

    AssertEq(Memmy_Process_ScanValue(arena, process, &options, value, Test_ScanSink(&results, arena), &error),
             Memmy_Status_Ok);
    Memmy_Addr expected[] = {0x102e};
    Test_AssertScanAddresses(&results, expected, ArrayCount(expected));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}
TestSuite suite_memmy_scan = TestSuite_Make(
    "Memmy Scan", TestCase_Make(Test_MemmyScanFindsBeginningMiddleAndEnd),
    TestCase_Make(Test_MemmyScanSinkReceivesMatchesInAddressOrder),
    TestCase_Make(Test_MemmyScanReusesOneTransientBufferAcrossRegions),
    TestCase_Make(Test_MemmyScanPropagatesSinkError),
    TestCase_Make(Test_MemmyScanDoesNotReadOutsideRequestedRangeAndAllowsZeroLength),
    TestCase_Make(Test_MemmyScanFindsChunkBoundaryMatchesAndHonorsLimit),
    TestCase_Make(Test_MemmyScanUsesRegionIntersectionWhenAvailable),
    TestCase_Make(Test_MemmyScanFindsPatternAcrossAdjacentReadableRegions),
    TestCase_Make(Test_MemmyScanRequiresRegionEnumeration),
    TestCase_Make(Test_MemmyScanSkipsUnreadableHolesAndReportsFullyUnreadableRange),
    TestCase_Make(Test_MemmyScanScansPartialReads), TestCase_Make(Test_MemmyScanSkipsNonReadableRegions),
    TestCase_Make(Test_MemmyValueScanFindsScalarValuesAtMultipleAlignments),
    TestCase_Make(Test_MemmyValueScanPointerWidthAware), TestCase_Make(Test_MemmyValueScanBytesUtf8AndUtf16),
    TestCase_Make(Test_MemmyValueScanRangeChunkLimitRegionAndReadErrors),
    TestCase_Make(Test_MemmyValueScanFindsValueAcrossAdjacentReadableRegions),
    TestCase_Make(Test_MemmyReferenceScanFindsPtr32AndPtr64),
    TestCase_Make(Test_MemmyReferenceScanFindsRel32PositiveNegativeAndAnyDeduped),
    TestCase_Make(Test_MemmyReferenceScanChunkLimitRegionsUnreadableAndInvalidArguments), );
