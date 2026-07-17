#ifndef TEST_MEMMY_BACKEND_H
#define TEST_MEMMY_BACKEND_H

#include "memmy.h"

#define TEST_MEMMY_BACKEND_MEMORY_SIZE 256
#define TEST_MEMMY_BACKEND_MAX_PROCESSES 16
#define TEST_MEMMY_BACKEND_MAX_MODULES 32
#define TEST_MEMMY_BACKEND_MAX_REGIONS 32
#define TEST_MEMMY_BACKEND_MAX_FUNCTIONS 32
#define TEST_MEMMY_BACKEND_MAX_UNREADABLE_RANGES 16

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

typedef struct Test_MemmyBackendUnreadableRange Test_MemmyBackendUnreadableRange;
struct Test_MemmyBackendUnreadableRange
{
    Memmy_Addr start;
    Memmy_Addr end;
};

typedef struct Test_MemmyBackendFunction Test_MemmyBackendFunction;
struct Test_MemmyBackendFunction
{
    U32 pid;
    Memmy_Range range;
};

typedef struct Test_MemmyBackend Test_MemmyBackend;
struct Test_MemmyBackend
{
    Memmy_Backend backend;
    Memmy_Addr memory_base;
    U8 memory[TEST_MEMMY_BACKEND_MEMORY_SIZE];
    Memmy_Status read_status;
    U64 read_limit;
    U64 read_call_count;
    void *first_read_buffer;
    B32 read_buffer_changed;
    U64 open_call_count;
    U64 close_call_count;
    U32 last_open_pid;
    U32 last_close_pid;
    Memmy_Addr min_read_addr;
    Memmy_Addr max_read_end;
    Memmy_Status write_status;
    U64 write_limit;
    B32 process_info_strings_use_enum_arena;
    Test_MemmyBackendProcess processes[TEST_MEMMY_BACKEND_MAX_PROCESSES];
    U64 process_count;
    Test_MemmyBackendModule modules[TEST_MEMMY_BACKEND_MAX_MODULES];
    U64 module_count;
    Test_MemmyBackendRegion regions[TEST_MEMMY_BACKEND_MAX_REGIONS];
    U64 region_count;
    Test_MemmyBackendFunction functions[TEST_MEMMY_BACKEND_MAX_FUNCTIONS];
    U64 function_count;
    Test_MemmyBackendUnreadableRange unreadable_ranges[TEST_MEMMY_BACKEND_MAX_UNREADABLE_RANGES];
    U64 unreadable_range_count;
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
Test_MemmyBackendFunction *Test_MemmyBackend_AddFunction(Test_MemmyBackend *backend, U32 pid, Memmy_Addr start,
                                                         Memmy_Addr end);
Test_MemmyBackendUnreadableRange *Test_MemmyBackend_AddUnreadableRange(Test_MemmyBackend *backend, Memmy_Addr start,
                                                                       Memmy_Addr end);
void Test_MemmyBackend_SetMemoryBase(Test_MemmyBackend *backend, Memmy_Addr base);
void Test_MemmyBackend_SetReadStatus(Test_MemmyBackend *backend, Memmy_Status status);
void Test_MemmyBackend_SetReadLimit(Test_MemmyBackend *backend, U64 limit);
void Test_MemmyBackend_SetWriteStatus(Test_MemmyBackend *backend, Memmy_Status status);
void Test_MemmyBackend_SetWriteLimit(Test_MemmyBackend *backend, U64 limit);

#endif // TEST_MEMMY_BACKEND_H
