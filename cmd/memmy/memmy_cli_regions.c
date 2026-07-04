#include "memmy_cli_internal.h"

#include "base_checked.h"

Memmy_Status Memmy_Cli_RunRegions(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListRegions, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("regions"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("regions"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_filter || options->has_addr || options->has_type || options->has_count)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for regions"), (String8){0});
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Query, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_RegionList regions = {0};
    status = Memmy_Process_ListRegions(arena, process, &regions, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    String8List lines = {0};
    if (options->json)
    {
        String8List_Push(arena, &lines, String8_Lit("[\n"));
    }
    else
    {
        String8List_Push(arena, &lines,
                         String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"));
    }
    B32 first_json = 1;
    List_ForEach(Memmy_Region, region, &regions.list, link)
    {
        Memmy_Addr end = 0;
        if (!AddU64Checked(region->base, region->size, &end))
        {
            Memmy_Process_Close(process);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("region"), String8_Lit("region end overflow"));
            return Memmy_Status_Overflow;
        }

        String8 access = Memmy_Cli_RegionAccessString(region->access);
        String8 state = Memmy_Cli_RegionStateString(region->state);
        String8 base = Memmy_Cli_FormatAddress(arena, process->pointer_width, region->base);
        String8 end_text = Memmy_Cli_FormatAddress(arena, process->pointer_width, end);
        if (options->json)
        {
            String8 access_json = Memmy_Cli_FormatJsonString(arena, access);
            String8 state_json = Memmy_Cli_FormatJsonString(arena, state);
            Memmy_Cli_PushLine(arena, &lines,
                               "%s  {\"base\":\"%.*s\",\"end\":\"%.*s\",\"size\":\"0x%llx\","
                               "\"access\":%.*s,\"state\":%.*s}",
                               first_json ? "" : ",\n", (int)base.len, (char *)base.data, (int)end_text.len,
                               (char *)end_text.data, (unsigned long long)region->size, (int)access_json.len,
                               (char *)access_json.data, (int)state_json.len, (char *)state_json.data);
            first_json = 0;
        }
        else
        {
            Memmy_Cli_PushLine(arena, &lines, "%.*s  %.*s  0x%llx     %.*s     %.*s\n", (int)base.len,
                               (char *)base.data, (int)end_text.len, (char *)end_text.data,
                               (unsigned long long)region->size, (int)access.len, (char *)access.data, (int)state.len,
                               (char *)state.data);
        }
    }
    if (options->json)
    {
        String8List_Push(arena, &lines, first_json ? String8_Lit("]\n") : String8_Lit("\n]\n"));
    }

    Memmy_Process_Close(process);
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}
