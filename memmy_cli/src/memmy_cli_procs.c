#include "memmy_cli_internal.h"

Memmy_Status Memmy_Cli_RunProcs(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("procs"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("procs"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_pid || options->has_name || options->has_addr || options->has_type || options->has_count)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for procs"), (String8){0});
    }

    Memmy_ProcessList processes = {0};
    status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8List lines = {0};
    if (options->json)
    {
        String8List_Push(arena, &lines, String8_Lit("[\n"));
    }
    else
    {
        String8List_Push(arena, &lines, String8_Lit("PID     ARCH   NAME\n"));
    }
    B32 first_json = 1;
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (options->has_filter && !Memmy_Cli_ContainsNoCase(info->name, options->filter) &&
            !Memmy_Cli_ContainsNoCase(info->path, options->filter))
        {
            continue;
        }

        if (options->json)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, info->name);
            String8 path = Memmy_Cli_FormatJsonString(arena, info->path);
            U32 pointer_width = info->pointer_width == Memmy_PointerWidth_32 ? 32 : 64;
            Memmy_Cli_PushLine(arena, &lines, "%s  {\"pid\":%u,\"name\":%.*s,\"path\":%.*s,\"pointer_width\":%u}",
                               first_json ? "" : ",\n", info->pid, (int)name.len, (char *)name.data, (int)path.len,
                               (char *)path.data, pointer_width);
            first_json = 0;
        }
        else
        {
            String8 arch = Memmy_Cli_PointerWidthString(info->pointer_width);
            Memmy_Cli_PushLine(arena, &lines, "%u    %.*s    %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                               (int)info->name.len, (char *)info->name.data);
        }
    }
    if (options->json)
    {
        String8List_Push(arena, &lines, first_json ? String8_Lit("]\n") : String8_Lit("\n]\n"));
    }
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}
