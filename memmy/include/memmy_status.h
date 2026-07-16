#ifndef MEMMY_STATUS_H
#define MEMMY_STATUS_H

#include "base.h"

typedef U32 Memmy_Status;
enum
{
    Memmy_Status_Ok,

    Memmy_Status_InvalidArgument,
    Memmy_Status_NotFound,
    Memmy_Status_Ambiguous,
    Memmy_Status_AccessDenied,
    Memmy_Status_PartialRead,
    Memmy_Status_PartialWrite,
    Memmy_Status_Unreadable,
    Memmy_Status_Unwritable,
    Memmy_Status_ParseError,
    Memmy_Status_Overflow,
    Memmy_Status_InvalidEncoding,
    Memmy_Status_Unsupported,
    Memmy_Status_PlatformError,
    Memmy_Status_OutOfMemory,
};

typedef struct Memmy_Error Memmy_Error;
struct Memmy_Error
{
    Memmy_Status status;
    U32 os_code;
    String8 message;
    String8 input;
    U64 byte_offset;
    U64 byte_count;
    String8 context;
};

char const *Memmy_Status_Name(Memmy_Status status);
String8 Memmy_Status_String(Memmy_Status status);
// Context and message strings are borrowed and must remain valid while the error is used.
void Memmy_Error_Set(Memmy_Error *error, Memmy_Status status, String8 context, String8 message);

#endif // MEMMY_STATUS_H
