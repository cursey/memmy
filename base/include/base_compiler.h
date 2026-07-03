#ifndef BASE_COMPILER_H
#define BASE_COMPILER_H

#include "base_core.h"

// ---------------------------------------------------------------------------
// Compiler intrinsics abstraction
// ---------------------------------------------------------------------------

U64 Compiler_PopcountU64(U64 x);
U64 Compiler_CtzU64(U64 x);
U64 Compiler_ClzU64(U64 x);

#endif // BASE_COMPILER_H
