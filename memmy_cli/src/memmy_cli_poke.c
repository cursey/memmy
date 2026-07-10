#include "memmy_cli_internal.h"

Memmy_Status MemmyCli_PokeOutput_Format(Arena *arena, MemmyCli_PokeOutput *poke, B32 jsonl, String8 *out,
                                        Memmy_Error *error)
{
    MemmyCli_ValueFormat format = {
        .type = poke->type,
        .type_text = poke->type_text.len != 0 ? poke->type_text : MemmyCli_Type_String(poke->type),
    };

    String8 old_value = {0};
    Memmy_Status status = MemmyCli_Value_Format(arena, &format, poke->old_bytes, &old_value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    String8 new_display = {0};
    status = MemmyCli_Value_Format(arena, &format, poke->new_bytes, &new_display, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8 address = MemmyCli_Address_Format(arena, poke->pointer_width, poke->address);
    if (jsonl)
    {
        String8 type_json = MemmyCli_JsonString_Format(arena, format.type_text);
        String8 old_json = MemmyCli_JsonString_Format(arena, old_value);
        String8 new_json = MemmyCli_JsonString_Format(arena, new_display);
        *out = String8_PushF(arena,
                             "{\"type\":\"poke\",\"process\":%u,\"address\":\"%.*s\",\"value_type\":%.*s,"
                             "\"old\":%.*s,\"new\":%.*s,\"dry_run\":%s}\n",
                             poke->pid, (int)address.len, (char *)address.data, (int)type_json.len,
                             (char *)type_json.data, (int)old_json.len, (char *)old_json.data, (int)new_json.len,
                             (char *)new_json.data, poke->dry_run ? "true" : "false");
    }
    else
    {
        String8List lines = {0};
        String8 verb = poke->dry_run ? String8_Lit("would write") : String8_Lit("wrote");
        MemmyCli_Line_Push(arena, &lines, "%.*s:\n", (int)verb.len, (char *)verb.data);
        MemmyCli_Line_Push(arena, &lines, "  process: %u\n", poke->pid);
        MemmyCli_Line_Push(arena, &lines, "  address: %.*s\n", (int)address.len, (char *)address.data);
        MemmyCli_Line_Push(arena, &lines, "  type:    %.*s\n", (int)format.type_text.len,
                           (char *)format.type_text.data);
        MemmyCli_Line_Push(arena, &lines, "  old:     %.*s\n", (int)old_value.len, (char *)old_value.data);
        MemmyCli_Line_Push(arena, &lines, "  new:     %.*s\n", (int)new_display.len, (char *)new_display.data);
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}
