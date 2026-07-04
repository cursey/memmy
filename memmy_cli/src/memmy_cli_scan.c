#include "memmy_cli_internal.h"

static String8 Memmy_Cli_FormatScanResults(Arena *arena, Memmy_ScanResultList *results,
                                           Memmy_PointerWidth pointer_width, B32 json, B32 jsonl)
{
    String8List lines = {0};
    if (jsonl)
    {
        List_ForEach(Memmy_ScanResult, result, &results->list, link)
        {
            String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, result->address);
            Memmy_Cli_PushLine(arena, &lines, "{\"address\":\"%.*s\"}\n", (int)address.len, (char *)address.data);
        }
    }
    else if (json)
    {
        String8List_Push(arena, &lines, String8_Lit("{\"results\":["));
        B32 first = 1;
        List_ForEach(Memmy_ScanResult, result, &results->list, link)
        {
            String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, result->address);
            Memmy_Cli_PushLine(arena, &lines, "%s{\"address\":\"%.*s\"}", first ? "" : ",", (int)address.len,
                               (char *)address.data);
            first = 0;
        }
        String8List_Push(arena, &lines, String8_Lit("]}\n"));
    }
    else
    {
        String8List_Push(arena, &lines, String8_Lit("ADDRESS\n"));
        List_ForEach(Memmy_ScanResult, result, &results->list, link)
        {
            String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, result->address);
            Memmy_Cli_PushLine(arena, &lines, "%.*s\n", (int)address.len, (char *)address.data);
        }
    }

    return String8List_Join(arena, &lines, (String8){0});
}

Memmy_Status Memmy_Cli_RunScan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_Read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectNonScanOptions(options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!options->has_type)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("scan"), String8_Lit("scan requires --type"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_value)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("scan"), String8_Lit("scan requires --value"));
        return Memmy_Status_ParseError;
    }

    Memmy_Range range = {0};
    status = Memmy_Cli_ResolveScanRange(options, String8_Lit("scan"), &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Read, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value value = {0};
    status = Memmy_Value_Parse(arena, options->type, process->pointer_width, options->value_text, &value, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    Memmy_ScanOptions scan_options = {
        .range = range,
        .limit = options->limit,
        .chunk_size = options->chunk_size,
    };
    Memmy_ScanResultList results = {0};
    Memmy_PointerWidth pointer_width = process->pointer_width;
    status = Memmy_Process_ScanValue(arena, process, &scan_options, value, &results, error);
    Memmy_Process_Close(process);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out = Memmy_Cli_FormatScanResults(arena, &results, pointer_width, options->json, options->jsonl);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RunPscan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_Read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectNonPscanOptions(options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!options->has_pattern)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("pscan"), String8_Lit("pscan requires --pattern"));
        return Memmy_Status_ParseError;
    }

    Memmy_Range range = {0};
    status = Memmy_Cli_ResolveScanRange(options, String8_Lit("pscan"), &range, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Read, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ScanOptions scan_options = {
        .range = range,
        .limit = options->limit,
        .chunk_size = options->chunk_size,
    };
    Memmy_ScanResultList results = {0};
    Memmy_PointerWidth pointer_width = process->pointer_width;
    status = Memmy_Process_ScanPattern(arena, process, &scan_options, options->pattern, &results, error);
    Memmy_Process_Close(process);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    *out = Memmy_Cli_FormatScanResults(arena, &results, pointer_width, options->json, options->jsonl);
    return Memmy_Status_Ok;
}
