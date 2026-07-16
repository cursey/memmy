#ifndef TEST_MEMMY_COMMON_H
#define TEST_MEMMY_COMMON_H

#include "base.h"
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

typedef struct Test_ProcessInfoNode Test_ProcessInfoNode;
struct Test_ProcessInfoNode
{
    ListLink link;
    Memmy_ProcessInfo info;
};

typedef struct Test_ProcessInfoList Test_ProcessInfoList;
struct Test_ProcessInfoList
{
    Arena *arena;
    List list; // Test_ProcessInfoNode
    Memmy_Status status;
};

typedef struct Test_ModuleNode Test_ModuleNode;
struct Test_ModuleNode
{
    ListLink link;
    Memmy_Module module;
};

typedef struct Test_ModuleList Test_ModuleList;
struct Test_ModuleList
{
    Arena *arena;
    List list; // Test_ModuleNode
    Memmy_Status status;
};

typedef struct Test_RegionNode Test_RegionNode;
struct Test_RegionNode
{
    ListLink link;
    Memmy_Region region;
};

typedef struct Test_RegionList Test_RegionList;
struct Test_RegionList
{
    Arena *arena;
    List list; // Test_RegionNode
    Memmy_Status status;
};

void Test_AssertBytes(String8 actual, U8 *expected, U64 expected_len);
void Test_ParseType(String8 text, Memmy_Type *out);
void Test_DisableEnumerateRegions(Test_MemmyBackend *backend);
void Test_ResetOpenTracking(Test_MemmyBackend *backend);
void Test_ParsePattern(Arena *arena, char *text, Memmy_Pattern *out);
void Test_ParseValue(Arena *arena, char *type_text, Memmy_PointerWidth pointer_width, char *value_text,
                     Memmy_Value *out);
void Test_OpenProcess(Arena *arena, Memmy_Process **out);
Memmy_ScanSink Test_ScanSink(Test_ScanResultList *results, Arena *arena);
Memmy_Status Test_ScanSinkCallback(void *user_data, Memmy_Addr address);
void Test_AssertScanAddresses(Test_ScanResultList *results, Memmy_Addr *addresses, U64 count);
Memmy_ProcessInfoSink Test_ProcessInfoSink(Test_ProcessInfoList *results, Arena *arena);
Memmy_Status Test_ProcessInfoSinkCallback(void *user_data, Memmy_ProcessInfo const *info);
Memmy_ModuleSink Test_ModuleSink(Test_ModuleList *results, Arena *arena);
Memmy_Status Test_ModuleSinkCallback(void *user_data, Memmy_Module const *module);
Memmy_RegionSink Test_RegionSink(Test_RegionList *results, Arena *arena);
Memmy_Status Test_RegionSinkCallback(void *user_data, Memmy_Region const *region);

#endif // TEST_MEMMY_COMMON_H
