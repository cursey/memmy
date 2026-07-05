#ifndef TEST_MEMMY_COMMON_H
#define TEST_MEMMY_COMMON_H

#include "base_list.h"
#include "base_os.h"
#include "memmy.h"
#include "memmy_cli.h"
#include "test_framework.h"
#include "test_memmy_backend.h"

#include <stdio.h>
#include <string.h>

typedef struct Test_ScanResult Test_ScanResult;
struct Test_ScanResult
{
    ListLink link;
    Memmy_Addr address;
};

typedef struct Test_ScanResultList Test_ScanResultList;
struct Test_ScanResultList
{
    Arena *arena;
    List list; // Test_ScanResult
    Memmy_Status status;
};

void Test_AssertBytes(String8 actual, U8 *expected, U64 expected_len);
void Test_ParseType(String8 text, Memmy_Type *out);
void Test_DisableListRegions(Test_MemmyBackend *backend);
void Test_ResetOpenTracking(Test_MemmyBackend *backend);
void Test_ParsePattern(Arena *arena, char *text, Memmy_Pattern *out);
void Test_ParseValue(Arena *arena, char *type_text, Memmy_PointerWidth pointer_width, char *value_text,
                     Memmy_Value *out);
void Test_OpenProcess(Arena *arena, Memmy_Process **out);
Memmy_ScanSink Test_ScanSink(Test_ScanResultList *results, Arena *arena);
Memmy_Status Test_ScanSinkCallback(void *user_data, Memmy_Addr address);
void Test_AssertScanAddresses(Test_ScanResultList *results, Memmy_Addr *addresses, U64 count);

#endif // TEST_MEMMY_COMMON_H
