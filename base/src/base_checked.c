#include "base_checked.h"

// ---------------------------------------------------------------------------
// Checked arithmetic
// ---------------------------------------------------------------------------

B32 AddU64Checked(U64 a, U64 b, U64 *out)
{
    if (a > U64_MAX - b)
    {
        return 0;
    }

    *out = a + b;
    return 1;
}

B32 SubU64Checked(U64 a, U64 b, U64 *out)
{
    if (a < b)
    {
        return 0;
    }

    *out = a - b;
    return 1;
}

B32 AddI64ToU64Checked(U64 a, I64 b, U64 *out)
{
    if (b >= 0)
    {
        return AddU64Checked(a, (U64)b, out);
    }

    U64 magnitude = (U64)(-(b + 1)) + 1;
    return SubU64Checked(a, magnitude, out);
}

B32 AddI64Checked(I64 a, I64 b, I64 *out)
{
    if (b > 0 && a > I64_MAX - b)
    {
        return 0;
    }
    if (b < 0 && a < I64_MIN - b)
    {
        return 0;
    }

    *out = a + b;
    return 1;
}

B32 SubI64Checked(I64 a, I64 b, I64 *out)
{
    if (b > 0 && a < I64_MIN + b)
    {
        return 0;
    }
    if (b < 0 && a > I64_MAX + b)
    {
        return 0;
    }

    *out = a - b;
    return 1;
}

B32 MulI64Checked(I64 a, I64 b, I64 *out)
{
    if (a == 0 || b == 0)
    {
        *out = 0;
        return 1;
    }

    if ((a == I64_MIN && b == -1) || (a == -1 && b == I64_MIN))
    {
        return 0;
    }

    if (a > 0)
    {
        if (b > 0)
        {
            if (a > I64_MAX / b)
            {
                return 0;
            }
        }
        else
        {
            if (b < I64_MIN / a)
            {
                return 0;
            }
        }
    }
    else
    {
        if (b > 0)
        {
            if (a < I64_MIN / b)
            {
                return 0;
            }
        }
        else
        {
            if (a < I64_MAX / b)
            {
                return 0;
            }
        }
    }

    *out = a * b;
    return 1;
}

B32 DivI64Checked(I64 a, I64 b, I64 *out)
{
    if (b == 0)
    {
        return 0;
    }
    if (a == I64_MIN && b == -1)
    {
        return 0;
    }

    *out = a / b;
    return 1;
}

B32 ModI64Checked(I64 a, I64 b, I64 *out)
{
    if (b == 0)
    {
        return 0;
    }
    if (a == I64_MIN && b == -1)
    {
        return 0;
    }

    *out = a % b;
    return 1;
}
