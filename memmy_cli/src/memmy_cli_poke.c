#include "memmy_cli_internal.h"

Memmy_Status Memmy_Cli_FormatPokeOutput(Arena *arena, Memmy_CliPokeOutput *poke, B32 json, String8 *out,
                                        Memmy_Error *error)
{
    Memmy_CliValueFormat format = {
        .type = poke->type,
        .type_text = poke->type_text.len != 0 ? poke->type_text : Memmy_Cli_TypeString(poke->type),
    };

    String8 old_value = {0};
    Memmy_Status status = Memmy_Cli_FormatValue(arena, &format, poke->old_bytes, &old_value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    String8 new_display = {0};
    status = Memmy_Cli_FormatValue(arena, &format, poke->new_bytes, &new_display, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8 address = Memmy_Cli_FormatAddress(arena, poke->pointer_width, poke->address);
    if (json)
    {
        String8 type_json = Memmy_Cli_FormatJsonString(arena, format.type_text);
        String8 old_json = Memmy_Cli_FormatJsonString(arena, old_value);
        String8 new_json = Memmy_Cli_FormatJsonString(arena, new_display);
        *out = String8_PushF(arena,
                             "{\"process\":%u,\"address\":\"%.*s\",\"type\":%.*s,\"old\":%.*s,\"new\":%.*s,"
                             "\"dry_run\":%s}\n",
                             poke->pid, (int)address.len, (char *)address.data, (int)type_json.len,
                             (char *)type_json.data, (int)old_json.len, (char *)old_json.data, (int)new_json.len,
                             (char *)new_json.data, poke->dry_run ? "true" : "false");
    }
    else
    {
        String8List lines = {0};
        String8 verb = poke->dry_run ? String8_Lit("would write") : String8_Lit("wrote");
        Memmy_Cli_PushLine(arena, &lines, "%.*s:\n", (int)verb.len, (char *)verb.data);
        Memmy_Cli_PushLine(arena, &lines, "  process: %u\n", poke->pid);
        Memmy_Cli_PushLine(arena, &lines, "  address: %.*s\n", (int)address.len, (char *)address.data);
        Memmy_Cli_PushLine(arena, &lines, "  type:    %.*s\n", (int)format.type_text.len,
                           (char *)format.type_text.data);
        Memmy_Cli_PushLine(arena, &lines, "  old:     %.*s\n", (int)old_value.len, (char *)old_value.data);
        Memmy_Cli_PushLine(arena, &lines, "  new:     %.*s\n", (int)new_display.len, (char *)new_display.data);
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}
