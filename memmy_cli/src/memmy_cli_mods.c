#include "memmy_cli_internal.h"

Memmy_Status Memmy_Cli_RunMods(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("mods"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("mods"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_addr || options->has_type || options->has_count)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for mods"), (String8){0});
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_ModuleList modules = {0};
    status = Memmy_Process_ListModules(arena, process, &modules, error);
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
        String8List_Push(arena, &lines, String8_Lit("BASE                SIZE        NAME\n"));
    }
    B32 first_json = 1;
    List_ForEach(Memmy_Module, module, &modules.list, link)
    {
        if (options->has_filter && !Memmy_Cli_ContainsNoCase(module->name, options->filter) &&
            !Memmy_Cli_ContainsNoCase(module->path, options->filter))
        {
            continue;
        }

        if (options->json)
        {
            String8 base = Memmy_Cli_FormatAddress(arena, process->pointer_width, module->base);
            String8 name = Memmy_Cli_FormatJsonString(arena, module->name);
            String8 path = Memmy_Cli_FormatJsonString(arena, module->path);
            Memmy_Cli_PushLine(arena, &lines, "%s  {\"base\":\"%.*s\",\"size\":\"0x%llx\",\"name\":%.*s,\"path\":%.*s}",
                               first_json ? "" : ",\n", (int)base.len, (char *)base.data,
                               (unsigned long long)module->size, (int)name.len, (char *)name.data, (int)path.len,
                               (char *)path.data);
            first_json = 0;
        }
        else
        {
            String8 base = Memmy_Cli_FormatAddress(arena, process->pointer_width, module->base);
            Memmy_Cli_PushLine(arena, &lines, "%.*s  0x%llx    %.*s\n", (int)base.len, (char *)base.data,
                               (unsigned long long)module->size, (int)module->name.len, (char *)module->name.data);
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
