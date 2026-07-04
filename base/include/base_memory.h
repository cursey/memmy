#ifndef BASE_MEMORY_H
#define BASE_MEMORY_H

#include "base_core.h"

void Memory_Copy(void *dst, void const *src, U64 size);
void Memory_Move(void *dst, void const *src, U64 size);
void Memory_Set(void *dst, U8 value, U64 size);
B32 Memory_Equals(void const *a, void const *b, U64 size);

#endif // BASE_MEMORY_H
