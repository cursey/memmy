#include "base_compiler.h"
#include "base_core.h"

#if COMPILER_MSVC
#include <intrin.h>

U64 Compiler_PopcountU64(U64 x)
{
    return (U64)__popcnt64(x);
}

U64 Compiler_CtzU64(U64 x)
{
    unsigned long index;
    _BitScanForward64(&index, x);
    return (U64)index;
}

U64 Compiler_ClzU64(U64 x)
{
    unsigned long index;
    _BitScanReverse64(&index, x);
    return (U64)(63 - index);
}

#else

U64 Compiler_PopcountU64(U64 x)
{
    return (U64)__builtin_popcountll(x);
}

U64 Compiler_CtzU64(U64 x)
{
    return (U64)__builtin_ctzll(x);
}

U64 Compiler_ClzU64(U64 x)
{
    return (U64)__builtin_clzll(x);
}

#endif
