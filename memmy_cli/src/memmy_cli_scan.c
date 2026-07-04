#include "memmy_cli_internal.h"

String8 Memmy_Cli_FormatScanResults(Arena *arena, Memmy_ScanResultList *results, Memmy_PointerWidth pointer_width,
                                    B32 json, B32 jsonl)
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
