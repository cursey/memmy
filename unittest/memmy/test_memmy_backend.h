#ifndef TEST_MEMMY_BACKEND_H
#define TEST_MEMMY_BACKEND_H

#include "memmy.h"

#define TEST_MEMMY_BACKEND_MEMORY_SIZE 256
#define TEST_MEMMY_BACKEND_MAX_PROCESSES 16
#define TEST_MEMMY_BACKEND_MAX_MODULES 32
#define TEST_MEMMY_BACKEND_MAX_REGIONS 32

typedef struct Test_MemmyBackendProcess Test_MemmyBackendProcess;
struct Test_MemmyBackendProcess
{
    U32 pid;
    String8 name;
    String8 path;
    Memmy_PointerWidth pointer_width;
};

typedef struct Test_MemmyBackendModule Test_MemmyBackendModule;
struct Test_MemmyBackendModule
{
    U32 pid;
    String8 name;
    String8 path;
    Memmy_Addr base;
    Memmy_Size size;
};

typedef struct Test_MemmyBackendRegion Test_MemmyBackendRegion;
struct Test_MemmyBackendRegion
{
    U32 pid;
    Memmy_Addr base;
    Memmy_Size size;
    Memmy_RegionAccess access;
    Memmy_RegionState state;
};

typedef struct Test_MemmyBackend Test_MemmyBackend;
struct Test_MemmyBackend
{
    Memmy_Backend backend;
    Memmy_Addr memory_base;
    U8 memory[TEST_MEMMY_BACKEND_MEMORY_SIZE];
    Memmy_Status read_status;
    U64 read_limit;
    Test_MemmyBackendProcess processes[TEST_MEMMY_BACKEND_MAX_PROCESSES];
    U64 process_count;
    Test_MemmyBackendModule modules[TEST_MEMMY_BACKEND_MAX_MODULES];
    U64 module_count;
    Test_MemmyBackendRegion regions[TEST_MEMMY_BACKEND_MAX_REGIONS];
    U64 region_count;
};

void Test_MemmyBackend_Init(Test_MemmyBackend *backend);
Memmy_Backend *Test_MemmyBackend_AsBackend(Test_MemmyBackend *backend);
Test_MemmyBackendProcess *Test_MemmyBackend_AddProcess(Test_MemmyBackend *backend, U32 pid, String8 name, String8 path,
                                                       Memmy_PointerWidth pointer_width);
Test_MemmyBackendModule *Test_MemmyBackend_AddModule(Test_MemmyBackend *backend, U32 pid, String8 name, String8 path,
                                                     Memmy_Addr base, Memmy_Size size);
Test_MemmyBackendRegion *Test_MemmyBackend_AddRegion(Test_MemmyBackend *backend, U32 pid, Memmy_Addr base,
                                                     Memmy_Size size, Memmy_RegionAccess access,
                                                     Memmy_RegionState state);
void Test_MemmyBackend_SetMemoryBase(Test_MemmyBackend *backend, Memmy_Addr base);
void Test_MemmyBackend_SetReadStatus(Test_MemmyBackend *backend, Memmy_Status status);
void Test_MemmyBackend_SetReadLimit(Test_MemmyBackend *backend, U64 limit);

#endif // TEST_MEMMY_BACKEND_H
