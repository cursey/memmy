#include "memmy_cli_internal.h"

String8 Memmy_Cli_FormatScanResults(Arena *arena, Memmy_ScanResultList *results, Memmy_PointerWidth pointer_width,
                                    B32 jsonl)
{
    String8List lines = {0};
    if (jsonl)
    {
        U64 count = 0;
        List_ForEach(Memmy_ScanResult, result, &results->list, link)
        {
            String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, result->address);
            Memmy_Cli_PushLine(arena, &lines, "{\"type\":\"match\",\"address\":\"%.*s\"}\n", (int)address.len,
                               (char *)address.data);
            count++;
        }
        Memmy_Cli_PushLine(arena, &lines, "{\"type\":\"summary\",\"matches\":%llu}\n", (unsigned long long)count);
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
