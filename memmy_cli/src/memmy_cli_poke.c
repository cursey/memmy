#include "memmy_cli_internal.h"

Memmy_Status Memmy_Cli_FormatPokeOutput(Arena *arena, Memmy_CliPokeOutput *poke, B32 json, String8 *out,
                                        Memmy_Error *error)
{
    Memmy_CliOptions format_options = {
        .type = poke->type,
        .type_text = poke->type_text.len != 0 ? poke->type_text : Memmy_Cli_TypeString(poke->type),
    };

    String8 old_value = {0};
    Memmy_Status status = Memmy_Cli_FormatValue(arena, &format_options, poke->old_bytes, &old_value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    String8 new_display = {0};
    status = Memmy_Cli_FormatValue(arena, &format_options, poke->new_bytes, &new_display, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8 address = Memmy_Cli_FormatAddress(arena, poke->pointer_width, poke->address);
    if (json)
    {
        String8 type_json = Memmy_Cli_FormatJsonString(arena, format_options.type_text);
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
        Memmy_Cli_PushLine(arena, &lines, "  type:    %.*s\n", (int)format_options.type_text.len,
                           (char *)format_options.type_text.data);
        Memmy_Cli_PushLine(arena, &lines, "  old:     %.*s\n", (int)old_value.len, (char *)old_value.data);
        Memmy_Cli_PushLine(arena, &lines, "  new:     %.*s\n", (int)new_display.len, (char *)new_display.data);
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RunPoke(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCaps(Memmy_BackendCap_Read | Memmy_BackendCap_Write, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("poke"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_filter)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for poke"), String8_Lit("--filter"));
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!options->has_addr)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("poke"), String8_Lit("poke requires --addr"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_type)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("poke"), String8_Lit("poke requires --type"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_value)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("poke"), String8_Lit("poke requires --value"));
        return Memmy_Status_ParseError;
    }
    if (options->has_count)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("poke"), String8_Lit("poke rejects --count"));
        return Memmy_Status_ParseError;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Value new_value = {0};
    status = Memmy_Value_Parse(arena, options->type, process->pointer_width, options->value_text, &new_value, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    U64 size = new_value.bytes.len;
    U8 *old_bytes = Arena_PushArrayNoZero(arena, U8, size);
    U64 byte_count = 0;
    Memmy_PointerWidth pointer_width = process->pointer_width;
    status = Memmy_Process_Read(process, options->addr, old_bytes, size, &byte_count, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }
    if (byte_count != size)
    {
        Memmy_Process_Close(process);
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("poke"), String8_Lit("partial old-value read"));
        return Memmy_Status_PartialRead;
    }

    Memmy_CliPokeOutput poke = {
        .pid = pid,
        .pointer_width = pointer_width,
        .address = options->addr,
        .type = options->type,
        .type_text = options->type_text,
        .old_bytes = String8_Make(old_bytes, size),
        .new_bytes = new_value.bytes,
        .dry_run = options->dry_run,
    };
    String8 formatted = {0};
    status = Memmy_Cli_FormatPokeOutput(arena, &poke, options->json, &formatted, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    if (!options->dry_run)
    {
        byte_count = 0;
        status =
            Memmy_Process_Write(process, options->addr, new_value.bytes.data, new_value.bytes.len, &byte_count, error);
        if (status != Memmy_Status_Ok)
        {
            Memmy_Process_Close(process);
            return status;
        }
        if (byte_count != new_value.bytes.len)
        {
            Memmy_Process_Close(process);
            Memmy_Error_Set(error, Memmy_Status_PartialWrite, String8_Lit("poke"), String8_Lit("partial write"));
            return Memmy_Status_PartialWrite;
        }
    }

    Memmy_Process_Close(process);
    *out = formatted;
    return Memmy_Status_Ok;
}
