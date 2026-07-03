#ifndef BASE_BITSET_H
#define BASE_BITSET_H

#include "base_arena.h"
#include "base_core.h"

// ---------------------------------------------------------------------------
// BitSet
// ---------------------------------------------------------------------------

typedef struct BitSet BitSet;
struct BitSet
{
    U64 *words;
    U64 count; // number of U64 words
};

BitSet BitSet_Alloc(Arena *a, U64 bit_count);
void BitSet_Set(BitSet *bs, U64 idx);
void BitSet_Clear(BitSet *bs, U64 idx);
B32 BitSet_Test(BitSet *bs, U64 idx);
void BitSet_Union(BitSet *dst, BitSet *src);
void BitSet_Intersect(BitSet *dst, BitSet *src);
void BitSet_Difference(BitSet *dst, BitSet *src);
void BitSet_ClearAll(BitSet *bs);
U64 BitSet_Popcount(BitSet *bs);
U64 BitSet_NextSet(BitSet *bs, U64 start);

#endif // BASE_BITSET_H
