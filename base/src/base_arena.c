#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_os.h"

// ---------------------------------------------------------------------------
// Arena allocator
// ---------------------------------------------------------------------------

Arena *Arena_Create(U64 reserve_size)
{
    void *mem = Os_MemReserve(reserve_size);
    if (mem == 0)
    {
        fprintf(stderr, "Arena_Create: failed to reserve %llu bytes\n", (unsigned long long)reserve_size);
        abort();
    }
    Os_MemCommit(mem, ARENA_COMMIT_CHUNK);
    Arena *a = (Arena *)mem;
    a->base = (U8 *)mem;
    a->pos = AlignUp(sizeof(Arena), 16);
    a->commit = ARENA_COMMIT_CHUNK;
    a->cap = reserve_size;
    return a;
}

Arena *Arena_CreateDefault(void)
{
    return Arena_Create(ARENA_DEFAULT_RESERVE);
}

void Arena_Destroy(Arena *a)
{
    U8 *base = a->base;
    U64 cap = a->cap;
    Os_MemRelease(base, cap);
}

void *Arena_Push(Arena *a, U64 size, U64 align)
{
    U64 pos_aligned = AlignUp(a->pos, align);
    U64 new_pos = pos_aligned + size;

    if (new_pos > a->commit)
    {
        U64 commit_new = AlignUp(new_pos, ARENA_COMMIT_CHUNK);
        if (commit_new > a->cap)
        {
            fprintf(stderr, "Arena_Push: out of memory (requested %llu, cap %llu)\n", (unsigned long long)new_pos,
                    (unsigned long long)a->cap);
            abort();
        }
        Os_MemCommit(a->base + a->commit, commit_new - a->commit);
        a->commit = commit_new;
    }

    void *result = a->base + pos_aligned;
    a->pos = new_pos;
    return result;
}

void *Arena_PushZero(Arena *a, U64 size, U64 align)
{
    void *result = Arena_Push(a, size, align);
    memset(result, 0, size);
    return result;
}

void Arena_PopTo(Arena *a, U64 pos)
{
    if (pos < AlignUp(sizeof(Arena), 16))
    {
        pos = AlignUp(sizeof(Arena), 16);
    }
    if (pos < a->pos)
    {
        a->pos = pos;
    }
}

void Arena_Clear(Arena *a)
{
    Arena_PopTo(a, AlignUp(sizeof(Arena), 16));
}

U64 Arena_Pos(Arena *a)
{
    return a->pos;
}

// ---------------------------------------------------------------------------
// Scratch arenas
// ---------------------------------------------------------------------------

#define SCRATCH_COUNT 2

static THREAD_LOCAL Arena *tl_scratch[SCRATCH_COUNT];

static void Scratch_EnsureInit(void)
{
    if (tl_scratch[0] == 0)
    {
        for (U64 i = 0; i < SCRATCH_COUNT; i++)
        {
            tl_scratch[i] = Arena_CreateDefault();
        }
    }
}

Scratch Scratch_Begin(Arena **conflicts, U64 conflict_count)
{
    Scratch_EnsureInit();
    Scratch s = {0};
    for (U64 i = 0; i < SCRATCH_COUNT; i++)
    {
        B32 is_conflict = 0;
        for (U64 j = 0; j < conflict_count; j++)
        {
            if (tl_scratch[i] == conflicts[j])
            {
                is_conflict = 1;
                break;
            }
        }
        if (!is_conflict)
        {
            s.arena = tl_scratch[i];
            s.pos = s.arena->pos;
            return s;
        }
    }
    fprintf(stderr, "Scratch_Begin: all scratch arenas conflicted\n");
    abort();
    return s;
}

void Scratch_End(Scratch s)
{
    Arena_PopTo(s.arena, s.pos);
}
