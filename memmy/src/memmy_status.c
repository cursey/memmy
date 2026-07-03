#include "memmy_status.h"

char *Memmy_Status_Name(Memmy_Status status)
{
    char *result = "unknown";
    switch (status)
    {
    case Memmy_Status_Ok:
        result = "ok";
        break;
    case Memmy_Status_InvalidArgument:
        result = "invalid_argument";
        break;
    case Memmy_Status_NotFound:
        result = "not_found";
        break;
    case Memmy_Status_Ambiguous:
        result = "ambiguous";
        break;
    case Memmy_Status_AccessDenied:
        result = "access_denied";
        break;
    case Memmy_Status_PartialRead:
        result = "partial_read";
        break;
    case Memmy_Status_PartialWrite:
        result = "partial_write";
        break;
    case Memmy_Status_Unreadable:
        result = "unreadable";
        break;
    case Memmy_Status_Unwritable:
        result = "unwritable";
        break;
    case Memmy_Status_ParseError:
        result = "parse_error";
        break;
    case Memmy_Status_Overflow:
        result = "overflow";
        break;
    case Memmy_Status_InvalidEncoding:
        result = "invalid_encoding";
        break;
    case Memmy_Status_Unsupported:
        result = "unsupported";
        break;
    case Memmy_Status_PlatformError:
        result = "platform_error";
        break;
    case Memmy_Status_OutOfMemory:
        result = "out_of_memory";
        break;
    }
    return result;
}

String8 Memmy_Status_String(Memmy_Status status)
{
    return String8_FromCStr(Memmy_Status_Name(status));
}

void Memmy_Error_Set(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    if (error != 0)
    {
        *error = (Memmy_Error){
            .status = status,
            .context = context,
            .message = message,
        };
    }
}
