#ifndef MEMMY_SCAN_H
#define MEMMY_SCAN_H

#include "base_arena.h"
#include "base_list.h"
#include "memmy_process.h"
#include "memmy_value.h"

#define MEMMY_DEFAULT_SCAN_CHUNK_SIZE Kilobytes(64)

typedef struct Memmy_ScanOptions Memmy_ScanOptions;
struct Memmy_ScanOptions
{
    Memmy_Range range;
    U64 limit;
    U64 chunk_size;
};

typedef struct Memmy_ScanResult Memmy_ScanResult;
struct Memmy_ScanResult
{
    ListLink link;
    Memmy_Addr address;
};

typedef struct Memmy_ScanResultList Memmy_ScanResultList;
struct Memmy_ScanResultList
{
    List list; // Memmy_ScanResult
};

Memmy_ScanResult *Memmy_ScanResultList_Push(Arena *arena, Memmy_ScanResultList *list);
Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern, Memmy_ScanResultList *out, Memmy_Error *error);

#endif // MEMMY_SCAN_H
