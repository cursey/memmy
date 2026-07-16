#ifndef MEMMY_TYPES_H
#define MEMMY_TYPES_H

#include "base.h"

typedef U64 Memmy_Addr;
typedef U64 Memmy_Size;

typedef struct Memmy_Range Memmy_Range;
struct Memmy_Range
{
    Memmy_Addr start;
    Memmy_Addr end;
};

#endif // MEMMY_TYPES_H
