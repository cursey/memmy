#include "base_process.h"

#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_list.h"
#include "base_os.h"
#include "base_string.h"

static char **BuildArgv(Arena *a, String8List argv)
{
    U64 n = argv.list.count;
    char **out = Arena_PushArray(a, char *, n + 1);
    U64 i = 0;
    List_ForEach(String8Node, node, &argv.list, link)
    {
        char *buf = Arena_PushArrayNoZero(a, char, node->str.len + 1);
        memcpy(buf, node->str.data, node->str.len);
        buf[node->str.len] = 0;
        out[i++] = buf;
    }
    out[n] = 0;
    return out;
}

Process_Result Process_Run(Arena *a, String8List argv, String8 stdin_input)
{
    Process_Result result = {0};
    if (argv.list.count == 0)
    {
        result.spawn_failed = 1;
        return result;
    }
    Scratch scratch = Scratch_Begin(&a, 1);
    Os_ProcSpawn spawn = {0};
    spawn.argv = BuildArgv(scratch.arena, argv);
    spawn.stdin_input = stdin_input;

    Os_ProcResult proc = {0};
    B32 ok = Os_ProcRun(a, spawn, &proc);
    Scratch_End(scratch);
    if (!ok)
    {
        result.spawn_failed = 1;
        return result;
    }
    result.stdout_bytes = proc.stdout_bytes;
    result.stderr_bytes = proc.stderr_bytes;
    result.exit_code = proc.exit_code;
    result.signaled = proc.signaled;
    return result;
}

Process_Result Process_RunPipeline(Arena *a, Process_PipelineStage *stages, U64 stage_count, String8 stdin_input)
{
    Process_Result result = {0};
    if (stage_count == 0)
    {
        result.spawn_failed = 1;
        return result;
    }

    String8 chained_input = stdin_input;
    B32 captured_first_fail = 0;
    I32 first_fail_code = 0;

    for (U64 i = 0; i < stage_count; i++)
    {
        Process_Result stage = Process_Run(a, stages[i].argv, chained_input);
        result.stdout_bytes = stage.stdout_bytes;
        result.stderr_bytes = stage.stderr_bytes;
        result.signaled = stage.signaled;
        result.spawn_failed = stage.spawn_failed;
        if (stage.spawn_failed)
        {
            result.exit_code = -1;
            return result;
        }
        if (!captured_first_fail && stage.exit_code != 0)
        {
            captured_first_fail = 1;
            first_fail_code = stage.exit_code;
        }
        chained_input = stage.stdout_bytes;
    }

    result.exit_code = captured_first_fail ? first_fail_code : 0;
    return result;
}
