#include "base_math.h"

B32 F32_IsFinite(F32 value)
{
    union {
        F32 f;
        U32 u;
    } bits = {.f = value};
    return (bits.u & 0x7f800000u) != 0x7f800000u;
}

B32 F64_IsFinite(F64 value)
{
    union {
        F64 f;
        U64 u;
    } bits = {.f = value};
    return (bits.u & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}
