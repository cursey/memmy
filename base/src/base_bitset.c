#include <string.h>

#include "base_arena.h"
#include "base_bitset.h"
#include "base_compiler.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// BitSet
// ---------------------------------------------------------------------------

BitSet BitSet_Alloc(Arena *a, U64 bit_count)
{
    U64 word_count = (bit_count + 63) / 64;
    BitSet bs;
    bs.words = Arena_PushArray(a, U64, word_count);
    bs.count = word_count;
    return bs;
}

void BitSet_Set(BitSet *bs, U64 idx)
{
    U64 word = idx / 64;
    U64 bit = idx % 64;
    if (word < bs->count)
    {
        bs->words[word] |= (U64)1 << bit;
    }
}

void BitSet_Clear(BitSet *bs, U64 idx)
{
    U64 word = idx / 64;
    U64 bit = idx % 64;
    if (word < bs->count)
    {
        bs->words[word] &= ~((U64)1 << bit);
    }
}

B32 BitSet_Test(BitSet *bs, U64 idx)
{
    U64 word = idx / 64;
    U64 bit = idx % 64;
    if (word >= bs->count)
    {
        return 0;
    }
    return (bs->words[word] >> bit) & 1;
}

void BitSet_Union(BitSet *dst, BitSet *src)
{
    U64 n = Min(dst->count, src->count);
    for (U64 i = 0; i < n; i++)
    {
        dst->words[i] |= src->words[i];
    }
}

void BitSet_Intersect(BitSet *dst, BitSet *src)
{
    U64 n = Min(dst->count, src->count);
    for (U64 i = 0; i < n; i++)
    {
        dst->words[i] &= src->words[i];
    }
    for (U64 i = n; i < dst->count; i++)
    {
        dst->words[i] = 0;
    }
}

void BitSet_Difference(BitSet *dst, BitSet *src)
{
    U64 n = Min(dst->count, src->count);
    for (U64 i = 0; i < n; i++)
    {
        dst->words[i] &= ~src->words[i];
    }
}

void BitSet_ClearAll(BitSet *bs)
{
    memset(bs->words, 0, bs->count * sizeof(U64));
}

U64 BitSet_Popcount(BitSet *bs)
{
    U64 total = 0;
    for (U64 i = 0; i < bs->count; i++)
    {
        total += Compiler_PopcountU64(bs->words[i]);
    }
    return total;
}

U64 BitSet_NextSet(BitSet *bs, U64 start)
{
    U64 word_idx = start / 64;
    U64 bit_idx = start % 64;

    if (word_idx >= bs->count)
    {
        return U64_MAX;
    }

    // Check remaining bits in the first word
    U64 masked = bs->words[word_idx] >> bit_idx;
    if (masked != 0)
    {
        return start + Compiler_CtzU64(masked);
    }

    // Check subsequent words
    for (U64 i = word_idx + 1; i < bs->count; i++)
    {
        if (bs->words[i] != 0)
        {
            return i * 64 + Compiler_CtzU64(bs->words[i]);
        }
    }

    return U64_MAX;
}
