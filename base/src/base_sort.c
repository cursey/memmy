#include <string.h>

#include "base_core.h"
#include "base_sort.h"

// ---------------------------------------------------------------------------
// Sort (quicksort + insertion Sort fallback)
// ---------------------------------------------------------------------------

#define SORT_INSERTION_THRESHOLD 16

static void Sort_Swap(U8 *a, U8 *b, U64 size)
{
    U8 tmp[256];
    while (size > 0)
    {
        U64 chunk = Min(size, sizeof(tmp));
        memcpy(tmp, a, chunk);
        memcpy(a, b, chunk);
        memcpy(b, tmp, chunk);
        a += chunk;
        b += chunk;
        size -= chunk;
    }
}

static void Sort_Insertion(U8 *base, U64 count, U64 elem_size, CmpFn cmp, void *ctx)
{
    for (U64 i = 1; i < count; i++)
    {
        U64 j = i;
        while (j > 0 && cmp(base + (j - 1) * elem_size, base + j * elem_size, ctx) > 0)
        {
            Sort_Swap(base + (j - 1) * elem_size, base + j * elem_size, elem_size);
            j--;
        }
    }
}

static void Sort_Quick(U8 *base, U64 count, U64 elem_size, CmpFn cmp, void *ctx)
{
    while (count > SORT_INSERTION_THRESHOLD)
    {
        // Median-of-three pivot
        U64 mid = count / 2;
        U64 last = count - 1;

        if (cmp(base + mid * elem_size, base, ctx) < 0)
        {
            Sort_Swap(base, base + mid * elem_size, elem_size);
        }
        if (cmp(base + last * elem_size, base, ctx) < 0)
        {
            Sort_Swap(base, base + last * elem_size, elem_size);
        }
        if (cmp(base + mid * elem_size, base + last * elem_size, ctx) < 0)
        {
            Sort_Swap(base + mid * elem_size, base + last * elem_size, elem_size);
        }

        // Pivot is now at last
        U8 *pivot = base + last * elem_size;
        U64 lo = 0;
        U64 hi = last;

        for (;;)
        {
            while (lo < hi && cmp(base + lo * elem_size, pivot, ctx) < 0)
            {
                lo++;
            }
            while (hi > lo && cmp(base + (hi - 1) * elem_size, pivot, ctx) >= 0)
            {
                hi--;
            }
            if (lo >= hi)
            {
                break;
            }
            Sort_Swap(base + lo * elem_size, base + (hi - 1) * elem_size, elem_size);
            lo++;
            hi--;
        }

        Sort_Swap(base + lo * elem_size, pivot, elem_size);

        // Recurse on smaller partition, loop on larger
        U64 left_count = lo;
        U64 right_count = count - lo - 1;
        U8 *right_base = base + (lo + 1) * elem_size;

        if (left_count < right_count)
        {
            Sort_Quick(base, left_count, elem_size, cmp, ctx);
            base = right_base;
            count = right_count;
        }
        else
        {
            Sort_Quick(right_base, right_count, elem_size, cmp, ctx);
            count = left_count;
        }
    }

    Sort_Insertion(base, count, elem_size, cmp, ctx);
}

void Sort(void *base, U64 count, U64 elem_size, CmpFn cmp, void *ctx)
{
    if (count <= 1)
    {
        return;
    }
    Sort_Quick((U8 *)base, count, elem_size, cmp, ctx);
}
