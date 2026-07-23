#ifndef BASE_ARENA_H
#define BASE_ARENA_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Arena
// ---------------------------------------------------------------------------

#define ARENA_DEFAULT_RESERVE Gigabytes(8)
#define ARENA_COMMIT_CHUNK Kilobytes(64)

typedef struct Arena Arena;
struct Arena
{
    U8 *base;
    U64 pos;
    U64 commit;
    U64 cap;
};

Arena *Arena_Create(U64 reserve_size);
Arena *Arena_CreateDefault(void);
void Arena_Destroy(Arena *a);

void *Arena_Push(Arena *a, U64 size, U64 align);
void *Arena_PushZero(Arena *a, U64 size, U64 align);
void *Arena_PushArraySized(Arena *a, U64 element_size, U64 count, U64 align, B32 zero_memory);
void Arena_PopTo(Arena *a, U64 pos);
void Arena_Clear(Arena *a);
U64 Arena_Pos(Arena *a);

#define Arena_PushStruct(a, T) (T *)Arena_PushZero((a), sizeof(T), _Alignof(T))
#define Arena_PushArray(a, T, n) (T *)Arena_PushArraySized((a), sizeof(T), (n), _Alignof(T), 1)
#define Arena_PushArrayNoZero(a, T, n) (T *)Arena_PushArraySized((a), sizeof(T), (n), _Alignof(T), 0)

// ---------------------------------------------------------------------------
// Scratch arenas
// ---------------------------------------------------------------------------

typedef struct Scratch Scratch;
struct Scratch
{
    Arena *arena;
    U64 pos;
};

Scratch Scratch_Begin(Arena **conflicts, U64 conflict_count);
void Scratch_End(Scratch s);
// Releases this thread's scratch arenas. Call before the thread exits; no scratch scope may be active.
// Repeated calls are safe.
void Scratch_ReleaseThread(void);

#endif // BASE_ARENA_H
