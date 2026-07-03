#ifndef BASE_OS_H
#define BASE_OS_H

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// OS virtual memory abstraction
// ---------------------------------------------------------------------------

void *Os_MemReserve(U64 size);
void Os_MemCommit(void *ptr, U64 size);
void Os_MemDecommit(void *ptr, U64 size);
void Os_MemRelease(void *ptr, U64 size);

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------

B32 Os_FileReadAll(Arena *a, String8 path, String8 *out_bytes);
B32 Os_FileWriteAll(String8 path, String8 bytes);
B32 Os_FileExists(String8 path);
B32 Os_FileDelete(String8 path);
B32 Os_DirCreate(String8 path);

// ---------------------------------------------------------------------------
// Directory iteration
// ---------------------------------------------------------------------------

typedef struct Os_DirIter Os_DirIter;

Os_DirIter *Os_DirIterBegin(Arena *a, String8 path);
B32 Os_DirIterNext(Os_DirIter *it, String8 *out_name, B32 *out_is_dir);
void Os_DirIterEnd(Os_DirIter *it);

// ---------------------------------------------------------------------------
// Standard streams
// ---------------------------------------------------------------------------

String8 Os_ReadStdin(Arena *a);
void Os_WriteStdout(String8 s);
void Os_WriteStderr(String8 s);

// ---------------------------------------------------------------------------
// Process spawn (raw primitive; base_process wraps this)
// ---------------------------------------------------------------------------

typedef struct Os_ProcSpawn Os_ProcSpawn;
struct Os_ProcSpawn
{
    char **argv;         // null-terminated
    String8 stdin_input; // empty = no stdin input
};

typedef struct Os_ProcResult Os_ProcResult;
struct Os_ProcResult
{
    String8 stdout_bytes;
    String8 stderr_bytes;
    I32 exit_code;
    B32 signaled;
};

B32 Os_ProcRun(Arena *a, Os_ProcSpawn spawn, Os_ProcResult *out);

// ---------------------------------------------------------------------------
// Environment and process info
// ---------------------------------------------------------------------------

String8 Os_GetEnv(Arena *a, String8 name);
U32 Os_GetProcessId(void);
String8 Os_TempDir(Arena *a);

#endif // BASE_OS_H
