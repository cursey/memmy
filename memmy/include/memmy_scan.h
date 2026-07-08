#ifndef MEMMY_SCAN_H
#define MEMMY_SCAN_H

#include "base_arena.h"
#include "memmy_process.h"
#include "memmy_value.h"

#define MEMMY_DEFAULT_SCAN_CHUNK_SIZE Kilobytes(64)

typedef Memmy_Status Memmy_ScanSinkFn(void *user_data, Memmy_Addr address);
typedef B32 Memmy_ScanMatchFn(void *user_data, Memmy_Addr address, U8 *bytes, U64 available);

typedef struct Memmy_ScanSink Memmy_ScanSink;
struct Memmy_ScanSink
{
    Memmy_ScanSinkFn *callback;
    void *user_data;
};

typedef struct Memmy_ScanOptions Memmy_ScanOptions;
struct Memmy_ScanOptions
{
    Memmy_Range range;
    U64 limit;
    U64 chunk_size;
    B32 scan_readable_regions;
};

typedef U32 Memmy_ReferenceScanMode;
enum
{
    Memmy_ReferenceScanMode_Ptr,
    Memmy_ReferenceScanMode_Rel32,
    Memmy_ReferenceScanMode_Any,
};

Memmy_Status Memmy_Process_ScanValue(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                     Memmy_Value value, Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern, Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_Process_ScanReferences(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                          Memmy_ReferenceScanMode mode, Memmy_Addr target, Memmy_ScanSink sink,
                                          Memmy_Error *error);
Memmy_Status Memmy_Process_ScanCustom(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options, U64 min_size,
                                      U64 max_size, Memmy_ScanMatchFn *match, void *user_data, Memmy_ScanSink sink,
                                      Memmy_Error *error);

#endif // MEMMY_SCAN_H
