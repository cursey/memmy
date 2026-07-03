#include "base_core.h"
#include "base_hash.h"

// ---------------------------------------------------------------------------
// Hash functions
// ---------------------------------------------------------------------------

U64 Hash_Fnv1a(U8 *data, U64 len)
{
    U64 h = 0xcbf29ce484222325ULL;
    for (U64 i = 0; i < len; i++)
    {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    if (h == 0)
    {
        h = 1;
    }
    return h;
}

U64 Hash_U64(U64 key)
{
    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;
    if (key == 0)
    {
        key = 1;
    }
    return key;
}
