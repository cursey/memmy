#ifndef BASE_SORT_H
#define BASE_SORT_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Sort
// ---------------------------------------------------------------------------

typedef I32 (*CmpFn)(void *a, void *b, void *ctx);

void Sort(void *base, U64 count, U64 elem_size, CmpFn cmp, void *ctx);

#endif // BASE_SORT_H
