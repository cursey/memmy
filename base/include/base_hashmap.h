#ifndef BASE_HASHMAP_H
#define BASE_HASHMAP_H

#include "base_arena.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// Intrusive hash map (segmented bucket directory, arena-friendly)
//
// Buckets are stored in a segmented directory: segments[i] holds 2^i bucket
// slots. Growth allocates a new segment — old segments are never relocated.
// ---------------------------------------------------------------------------

#define HASHMAP_MAX_SEGMENTS 32
#define HASHMAP_LOAD_FACTOR_NUM 7
#define HASHMAP_LOAD_FACTOR_DEN 10

typedef struct HashLink HashLink;
struct HashLink
{
    HashLink *next;
    U64 hash;
};

typedef struct HashMap HashMap;
struct HashMap
{
    Arena *arena;
    HashLink **segments[HASHMAP_MAX_SEGMENTS];
    U64 bucket_count;
    U64 count;
};

typedef B32 (*HashMapEqFn)(void *link, void *ctx);

HashMap HashMap_Create(Arena *arena);
HashLink *HashMap_Find(HashMap *map, U64 hash, HashMapEqFn eq, void *ctx);
void HashMap_Insert(HashMap *map, HashLink *link);
void HashMap_Remove(HashMap *map, HashLink *link);

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

HashLink *HashMap_First(HashMap *map);
HashLink *HashMap_Next(HashMap *map, HashLink *link);

#define HashMap_ForEach(T, var, map, member)                                                                           \
    for (T *var = (HashMap_First(map) != 0 ? ContainerOf(HashMap_First(map), T, member) : (T *)0); var != 0;           \
         var = (HashMap_Next((map), &var->member) != 0 ? ContainerOf(HashMap_Next((map), &var->member), T, member)     \
                                                       : (T *)0))

#endif // BASE_HASHMAP_H
