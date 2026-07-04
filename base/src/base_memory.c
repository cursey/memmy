#include "base_memory.h"

#include <string.h>

void Memory_Copy(void *dst, void const *src, U64 size)
{
    if (size != 0)
    {
        memcpy(dst, src, (size_t)size);
    }
}

void Memory_Move(void *dst, void const *src, U64 size)
{
    if (size != 0)
    {
        memmove(dst, src, (size_t)size);
    }
}

void Memory_Set(void *dst, U8 value, U64 size)
{
    if (size != 0)
    {
        memset(dst, value, (size_t)size);
    }
}

B32 Memory_Equals(void const *a, void const *b, U64 size)
{
    B32 result = 1;
    if (size != 0)
    {
        result = memcmp(a, b, (size_t)size) == 0;
    }
    return result;
}
