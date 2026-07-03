#include "base_hashmap.h"

#include "base_arena.h"
#include "base_compiler.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// Segmented bucket directory helpers
//
// Bucket index i maps to:
//   segment = 63 - clz(i + 1)       (highest set bit position)
//   offset  = i + 1 - (1 << segment)
//
// Segment k holds 2^k bucket slots. Total capacity with k segments
// allocated = 2^k - 1 buckets.
// ---------------------------------------------------------------------------

static HashLink **HashMap_Bucket(HashMap *map, U64 index)
{
    U64 n = index + 1;
    U64 seg = 63 - Compiler_ClzU64(n);
    U64 off = n - ((U64)1 << seg);
    return &map->segments[seg][off];
}

static void HashMap_Grow(HashMap *map)
{
    // Current bucket_count is 2^level - 1. Allocating the next segment
    // doubles capacity: 2^(level+1) - 1.
    U64 level;
    if (map->bucket_count == 0)
    {
        level = 0;
    }
    else
    {
        level = 64 - Compiler_ClzU64(map->bucket_count);
    }

    U64 seg_size = (U64)1 << level;
    map->segments[level] = Arena_PushArray(map->arena, HashLink *, seg_size);
    U64 old_count = map->bucket_count;
    map->bucket_count = old_count + seg_size;

    // Rehash entries from existing buckets that may need to split.
    // An entry in bucket i might now belong in bucket (i + old_count)
    // if the newly significant bit is set in its hash.
    for (U64 i = 0; i < old_count; i++)
    {
        HashLink **slot = HashMap_Bucket(map, i);
        HashLink *prev = 0;
        HashLink *cur = *slot;
        while (cur != 0)
        {
            HashLink *next = cur->next;
            U64 new_idx = cur->hash % map->bucket_count;
            if (new_idx != i)
            {
                // Unlink from old bucket
                if (prev != 0)
                {
                    prev->next = next;
                }
                else
                {
                    *slot = next;
                }
                // Insert into new bucket
                HashLink **new_slot = HashMap_Bucket(map, new_idx);
                cur->next = *new_slot;
                *new_slot = cur;
            }
            else
            {
                prev = cur;
            }
            cur = next;
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

HashMap HashMap_Create(Arena *arena)
{
    HashMap map = {0};
    map.arena = arena;
    return map;
}

HashLink *HashMap_Find(HashMap *map, U64 hash, HashMapEqFn eq, void *ctx)
{
    if (map->bucket_count == 0)
    {
        return 0;
    }
    U64 idx = hash % map->bucket_count;
    HashLink *cur = *HashMap_Bucket(map, idx);
    while (cur != 0)
    {
        if (cur->hash == hash && eq(cur, ctx))
        {
            return cur;
        }
        cur = cur->next;
    }
    return 0;
}

void HashMap_Insert(HashMap *map, HashLink *link)
{
    // Grow if needed (or if this is the first insert)
    while (map->bucket_count == 0 ||
           map->count * HASHMAP_LOAD_FACTOR_DEN >= map->bucket_count * HASHMAP_LOAD_FACTOR_NUM)
    {
        HashMap_Grow(map);
    }

    U64 idx = link->hash % map->bucket_count;
    HashLink **slot = HashMap_Bucket(map, idx);
    link->next = *slot;
    *slot = link;
    map->count++;
}

void HashMap_Remove(HashMap *map, HashLink *link)
{
    U64 idx = link->hash % map->bucket_count;
    HashLink **slot = HashMap_Bucket(map, idx);
    HashLink *prev = 0;
    HashLink *cur = *slot;
    while (cur != 0)
    {
        if (cur == link)
        {
            if (prev != 0)
            {
                prev->next = cur->next;
            }
            else
            {
                *slot = cur->next;
            }
            link->next = 0;
            map->count--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

// ---------------------------------------------------------------------------
// Iteration
// ---------------------------------------------------------------------------

HashLink *HashMap_First(HashMap *map)
{
    for (U64 i = 0; i < map->bucket_count; i++)
    {
        HashLink *head = *HashMap_Bucket(map, i);
        if (head != 0)
        {
            return head;
        }
    }
    return 0;
}

HashLink *HashMap_Next(HashMap *map, HashLink *link)
{
    // Next in same chain
    if (link->next != 0)
    {
        return link->next;
    }
    // Find next non-empty bucket
    U64 idx = link->hash % map->bucket_count;
    for (U64 i = idx + 1; i < map->bucket_count; i++)
    {
        HashLink *head = *HashMap_Bucket(map, i);
        if (head != 0)
        {
            return head;
        }
    }
    return 0;
}
