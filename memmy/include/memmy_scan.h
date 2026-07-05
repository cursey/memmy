#ifndef MEMMY_SCAN_H
#define MEMMY_SCAN_H

#include "base_arena.h"
#include "memmy_process.h"
#include "memmy_value.h"

#define MEMMY_DEFAULT_SCAN_CHUNK_SIZE Kilobytes(64)

typedef Memmy_Status Memmy_ScanSinkFn(void *user_data, Memmy_Addr address);

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
};

Memmy_Status Memmy_Process_ScanValue(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                     Memmy_Value value, Memmy_ScanSink sink, Memmy_Error *error);
Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern, Memmy_ScanSink sink, Memmy_Error *error);

#endif // MEMMY_SCAN_H
