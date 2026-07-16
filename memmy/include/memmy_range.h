#ifndef MEMMY_RANGE_H
#define MEMMY_RANGE_H

#include "base.h"
#include "memmy_status.h"
#include "memmy_types.h"

Memmy_Status Memmy_Address_Parse(String8 text, Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_Size_Parse(String8 text, Memmy_Size *out, Memmy_Error *error);
Memmy_Status Memmy_Range_FromStartEnd(Memmy_Addr start, Memmy_Addr end, Memmy_Range *out, Memmy_Error *error);
Memmy_Status Memmy_Range_FromStartLength(Memmy_Addr start, Memmy_Size length, Memmy_Range *out, Memmy_Error *error);

#endif // MEMMY_RANGE_H
