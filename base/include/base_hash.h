#ifndef BASE_HASH_H
#define BASE_HASH_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Hash functions
// ---------------------------------------------------------------------------

U64 Hash_Fnv1a(U8 *data, U64 len);
U64 Hash_U64(U64 key);

#endif // BASE_HASH_H
