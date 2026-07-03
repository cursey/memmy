#ifndef TEST_MEMMY_BACKEND_H
#define TEST_MEMMY_BACKEND_H

#include "memmy.h"

#define TEST_MEMMY_BACKEND_MEMORY_SIZE 256

typedef struct Test_MemmyBackend Test_MemmyBackend;
struct Test_MemmyBackend
{
    Memmy_Backend backend;
    Memmy_PointerWidth pointer_width;
    Memmy_Addr memory_base;
    U8 memory[TEST_MEMMY_BACKEND_MEMORY_SIZE];
};

void Test_MemmyBackend_Init(Test_MemmyBackend *backend);
Memmy_Backend *Test_MemmyBackend_AsBackend(Test_MemmyBackend *backend);

#endif // TEST_MEMMY_BACKEND_H
