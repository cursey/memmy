#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base_arena.h"
#include "base_checked.h"
#include "base_core.h"
#include "base_os.h"

// ---------------------------------------------------------------------------
// Arena allocator
// ---------------------------------------------------------------------------

Arena *Arena_Create(U64 reserve_size)
{
    U64 header_size = AlignUp(sizeof(Arena), 16);
    if (reserve_size < header_size)
    {
        fprintf(stderr, "Arena_Create: reserve size %llu is smaller than the arena header\n",
                (unsigned long long)reserve_size);
        abort();
    }

    void *mem = Os_MemReserve(reserve_size);
    if (mem == 0)
    {
        fprintf(stderr, "Arena_Create: failed to reserve %llu bytes\n", (unsigned long long)reserve_size);
        abort();
    }
    U64 initial_commit = Min(reserve_size, ARENA_COMMIT_CHUNK);
    if (!Os_MemCommit(mem, initial_commit))
    {
        fprintf(stderr, "Arena_Create: failed to commit %llu bytes\n", (unsigned long long)initial_commit);
        Os_MemRelease(mem, reserve_size);
        abort();
    }
    Arena *a = (Arena *)mem;
    a->base = (U8 *)mem;
    a->pos = header_size;
    a->commit = initial_commit;
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
    if (align == 0 || (align & (align - 1)) != 0)
    {
        fprintf(stderr, "Arena_Push: alignment %llu is not a nonzero power of two\n", (unsigned long long)align);
        abort();
    }

    U64 padding = (0 - a->pos) & (align - 1);
    U64 pos_aligned = 0;
    U64 new_pos = 0;
    if (!AddU64Checked(a->pos, padding, &pos_aligned) || !AddU64Checked(pos_aligned, size, &new_pos))
    {
        fprintf(stderr, "Arena_Push: allocation size overflow\n");
        abort();
    }

    if (new_pos > a->cap)
    {
        fprintf(stderr, "Arena_Push: out of memory (requested %llu, cap %llu)\n", (unsigned long long)new_pos,
                (unsigned long long)a->cap);
        abort();
    }

    if (new_pos > a->commit)
    {
        U64 commit_rounded = 0;
        U64 commit_new = a->cap;
        if (AddU64Checked(new_pos, ARENA_COMMIT_CHUNK - 1, &commit_rounded))
        {
            commit_new = Min(commit_rounded & ~(ARENA_COMMIT_CHUNK - 1), a->cap);
        }
        if (!Os_MemCommit(a->base + a->commit, commit_new - a->commit))
        {
            fprintf(stderr, "Arena_Push: failed to commit %llu bytes\n", (unsigned long long)(commit_new - a->commit));
            abort();
        }
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

void *Arena_PushArraySized(Arena *a, U64 element_size, U64 count, U64 align, B32 zero_memory)
{
    U64 size = 0;
    if (!MulU64Checked(element_size, count, &size))
    {
        fprintf(stderr, "Arena_PushArraySized: allocation size overflow\n");
        abort();
    }
    return zero_memory ? Arena_PushZero(a, size, align) : Arena_Push(a, size, align);
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

void Scratch_ReleaseThread(void)
{
    for (U64 i = 0; i < SCRATCH_COUNT; i++)
    {
        if (tl_scratch[i] != 0)
        {
            Arena_Destroy(tl_scratch[i]);
            tl_scratch[i] = 0;
        }
    }
}
