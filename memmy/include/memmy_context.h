#ifndef MEMMY_CONTEXT_H
#define MEMMY_CONTEXT_H

#include "base_arena.h"
#include "memmy_backend.h"
#include "memmy_status.h"

typedef struct Memmy_Context Memmy_Context;
struct Memmy_Context
{
    Memmy_Backend *backend;
};

Memmy_Context *Memmy_Context_Get(void);
void Memmy_Context_Set(Memmy_Context *ctx);
Memmy_Context *Memmy_Context_Push(Memmy_Context *ctx);
void Memmy_Context_Pop(Memmy_Context *old_ctx);
Memmy_Status Memmy_Context_InitDefault(Arena *arena, Memmy_Context *ctx, Memmy_Error *error);

#endif // MEMMY_CONTEXT_H
