#include "memmy_cli_internal.h"

Memmy_Status Memmy_CliScanOutput_Begin(Memmy_CliScanOutput *output, Arena *arena, Memmy_CliOutputWriter writer,
                                       Memmy_PointerWidth pointer_width, B32 jsonl)
{
    *output = (Memmy_CliScanOutput){
        .arena = arena,
        .writer = writer,
        .pointer_width = pointer_width,
        .jsonl = jsonl,
    };
    if (!jsonl)
    {
        return writer.write(writer.user_data, String8_Lit("ADDRESS\n"));
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_CliScanOutput_PushMatch(void *user_data, Memmy_Addr address)
{
    Memmy_CliScanOutput *output = (Memmy_CliScanOutput *)user_data;
    String8 address_text = Memmy_Cli_FormatAddress(output->arena, output->pointer_width, address);
    String8 line = {0};
    if (output->jsonl)
    {
        line = String8_PushF(output->arena, "{\"type\":\"match\",\"address\":\"%.*s\"}\n", (int)address_text.len,
                             (char *)address_text.data);
    }
    else
    {
        line = String8_PushF(output->arena, "%.*s\n", (int)address_text.len, (char *)address_text.data);
    }
    output->count++;
    return output->writer.write(output->writer.user_data, line);
}

Memmy_Status Memmy_CliScanOutput_End(Memmy_CliScanOutput *output)
{
    if (output->jsonl)
    {
        String8 line = String8_PushF(output->arena, "{\"type\":\"summary\",\"matches\":%llu}\n",
                                     (unsigned long long)output->count);
        return output->writer.write(output->writer.user_data, line);
    }
    return Memmy_Status_Ok;
}
