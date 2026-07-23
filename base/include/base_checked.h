#ifndef BASE_CHECKED_H
#define BASE_CHECKED_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Checked arithmetic
// ---------------------------------------------------------------------------

B32 AddU64Checked(U64 a, U64 b, U64 *out);
B32 SubU64Checked(U64 a, U64 b, U64 *out);
B32 MulU64Checked(U64 a, U64 b, U64 *out);
B32 AddI64ToU64Checked(U64 a, I64 b, U64 *out);
B32 AddI64Checked(I64 a, I64 b, I64 *out);
B32 SubI64Checked(I64 a, I64 b, I64 *out);
B32 MulI64Checked(I64 a, I64 b, I64 *out);
B32 DivI64Checked(I64 a, I64 b, I64 *out);
B32 ModI64Checked(I64 a, I64 b, I64 *out);

#endif // BASE_CHECKED_H
