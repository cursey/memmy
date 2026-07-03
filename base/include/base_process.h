#ifndef BASE_PROCESS_H
#define BASE_PROCESS_H

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// Process
//
// Higher-level wrapper over Os_ProcRun. Accepts argv as String8List so callers
// don't have to build a char** themselves.
// ---------------------------------------------------------------------------

typedef struct Process_Result Process_Result;
struct Process_Result
{
    String8 stdout_bytes;
    String8 stderr_bytes;
    I32 exit_code;
    B32 signaled;
    B32 spawn_failed; // distinguishes spawn failure from non-zero exit
};

Process_Result Process_Run(Arena *a, String8List argv, String8 stdin_input);

// ---------------------------------------------------------------------------
// Pipelines (sequential: each stage's stdout feeds the next stage's stdin)
//
// `exit_code` is the first non-zero stage's exit, or the last stage's exit
// if all succeeded. `stdout_bytes` / `stderr_bytes` are from the last stage
// that ran.
// ---------------------------------------------------------------------------

typedef struct Process_PipelineStage Process_PipelineStage;
struct Process_PipelineStage
{
    String8List argv;
};

Process_Result Process_RunPipeline(Arena *a, Process_PipelineStage *stages, U64 stage_count, String8 stdin_input);

#endif // BASE_PROCESS_H
