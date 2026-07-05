#ifndef MEMMY_CLI_INTERNAL_H
#define MEMMY_CLI_INTERNAL_H

#include "memmy_cli.h"

typedef struct Memmy_CliOptions Memmy_CliOptions;
struct Memmy_CliOptions
{
    String8 input_path;
    B32 help;
    B32 version;
    B32 jsonl;
    B32 has_pid;
    U32 pid;
    B32 has_name;
    String8 name;
    B32 has_expr;
    String8 expr_text;
};

typedef struct Memmy_CliValueFormat Memmy_CliValueFormat;
struct Memmy_CliValueFormat
{
    Memmy_Type type;
    String8 type_text;
};

typedef struct Memmy_CliPeekOutput Memmy_CliPeekOutput;
struct Memmy_CliPeekOutput
{
    Memmy_PointerWidth pointer_width;
    Memmy_Addr address;
    Memmy_Type type;
    String8 type_text;
    String8 bytes;
};

typedef struct Memmy_CliPokeOutput Memmy_CliPokeOutput;
struct Memmy_CliPokeOutput
{
    U32 pid;
    Memmy_PointerWidth pointer_width;
    Memmy_Addr address;
    Memmy_Type type;
    String8 type_text;
    String8 old_bytes;
    String8 new_bytes;
    B32 dry_run;
};

void Memmy_Cli_PushLine(Arena *arena, String8List *list, char *fmt, ...);
Memmy_Status Memmy_Cli_InvalidOption(Memmy_Error *error, String8 message, String8 input);
Memmy_Status Memmy_Cli_RunReplStringWithOptions(Arena *arena, Memmy_CliOptions *base_options, String8 input,
                                                String8 *out, Memmy_Error *error);

Memmy_Status Memmy_Cli_RunExpr(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error);

Memmy_Status Memmy_Cli_FormatValue(Arena *arena, Memmy_CliValueFormat *format, String8 bytes, String8 *out,
                                   Memmy_Error *error);
String8 Memmy_Cli_TypeString(Memmy_Type type);
Memmy_Status Memmy_Cli_FormatPeekOutput(Arena *arena, Memmy_CliPeekOutput *peek, B32 jsonl, String8 *out,
                                        Memmy_Error *error);
Memmy_Status Memmy_Cli_FormatPokeOutput(Arena *arena, Memmy_CliPokeOutput *poke, B32 jsonl, String8 *out,
                                        Memmy_Error *error);
String8 Memmy_Cli_FormatScanResults(Arena *arena, Memmy_ScanResultList *results, Memmy_PointerWidth pointer_width,
                                    B32 jsonl);

#endif // MEMMY_CLI_INTERNAL_H
