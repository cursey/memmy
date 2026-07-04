#ifndef MEMMY_CLI_INTERNAL_H
#define MEMMY_CLI_INTERNAL_H

#include "memmy_cli.h"

typedef struct Memmy_CliOptions Memmy_CliOptions;
struct Memmy_CliOptions
{
    String8 command;
    B32 help;
    B32 version;
    B32 json;
    B32 jsonl;
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    B32 has_filter;
    String8 filter;
    B32 has_addr;
    Memmy_Addr addr;
    B32 has_type;
    String8 type_text;
    Memmy_Type type;
    B32 has_count;
    Memmy_Size count;
    B32 has_value;
    String8 value_text;
    B32 dry_run;
    B32 has_start;
    String8 start_text;
    Memmy_Addr start;
    B32 has_end;
    String8 end_text;
    Memmy_Addr end;
    B32 has_length;
    String8 length_text;
    Memmy_Size length;
    B32 has_limit;
    Memmy_Size limit;
    B32 has_chunk_size;
    Memmy_Size chunk_size;
    B32 has_pattern;
    String8 pattern_text;
    Memmy_Pattern pattern;
};

void Memmy_Cli_PushLine(Arena *arena, String8List *list, char *fmt, ...);
B32 Memmy_Cli_ContainsNoCase(String8 text, String8 needle);
String8 Memmy_Cli_PointerWidthString(Memmy_PointerWidth width);
String8 Memmy_Cli_RegionAccessString(Memmy_RegionAccess access);
String8 Memmy_Cli_RegionStateString(Memmy_RegionState state);
Memmy_Status Memmy_Cli_InvalidOption(Memmy_Error *error, String8 message, String8 input);
Memmy_Status Memmy_Cli_RequireCap(Memmy_BackendCap cap, Memmy_Error *error);
Memmy_Status Memmy_Cli_RequireCaps(Memmy_BackendCap caps, Memmy_Error *error);
Memmy_Status Memmy_Cli_ResolveTarget(Arena *arena, Memmy_CliOptions *options, U32 *out_pid, Memmy_Error *error);
Memmy_Status Memmy_Cli_RejectPokeOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error);
Memmy_Status Memmy_Cli_RejectScanOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error);
Memmy_Status Memmy_Cli_ResolveScanRange(Memmy_CliOptions *options, String8 command, Memmy_Range *out,
                                        Memmy_Error *error);
Memmy_Status Memmy_Cli_RejectNonPscanOptions(Memmy_CliOptions *options, Memmy_Error *error);
Memmy_Status Memmy_Cli_RejectNonScanOptions(Memmy_CliOptions *options, Memmy_Error *error);

Memmy_Status Memmy_Cli_RunProcs(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunPeek(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunPoke(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunScan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunPscan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunMods(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);
Memmy_Status Memmy_Cli_RunRegions(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);

Memmy_Status Memmy_Cli_FormatValue(Arena *arena, Memmy_CliOptions *options, String8 bytes, String8 *out,
                                   Memmy_Error *error);

#endif // MEMMY_CLI_INTERNAL_H
