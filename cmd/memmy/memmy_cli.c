#include "memmy_cli.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "base_checked.h"

static void Memmy_Cli_SetRangeError(Memmy_Error *error, String8 message)
{
    Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("range"), message);
}

static B32 Memmy_Cli_IsOption(char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-';
}

static Memmy_Status Memmy_Cli_ReadOptionValue(I32 argc, char **argv, I32 index, String8 option, String8 *out,
                                              Memmy_Error *error)
{
    if (index + 1 >= argc || Memmy_Cli_IsOption(argv[index + 1]))
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("range"),
                        String8_Lit("missing range option value"));
        if (error != 0)
        {
            error->input = option;
        }
        return Memmy_Status_ParseError;
    }

    *out = String8_FromCStr(argv[index + 1]);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ReadOptionValueRaw(I32 argc, char **argv, I32 index, String8 option, String8 *out,
                                                 Memmy_Error *error)
{
    if (index + 1 >= argc)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("missing option value"));
        if (error != 0)
        {
            error->input = option;
        }
        return Memmy_Status_ParseError;
    }

    *out = String8_FromCStr(argv[index + 1]);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_InvalidOption(Memmy_Error *error, String8 message, String8 input)
{
    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), message);
    if (error != 0)
    {
        error->input = input;
    }
    return Memmy_Status_InvalidArgument;
}

Memmy_Status Memmy_Cli_ParseRangeOptions(I32 argc, char **argv, Memmy_Range *out, Memmy_Error *error)
{
    String8 start_text = {0};
    String8 end_text = {0};
    String8 length_text = {0};
    B32 has_start = 0;
    B32 has_end = 0;
    B32 has_length = 0;

    for (I32 i = 0; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, String8_Lit("--start")))
        {
            if (has_start)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --start"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &start_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_start = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--end")))
        {
            if (has_end)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --end"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &end_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_end = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--length")))
        {
            if (has_length)
            {
                Memmy_Cli_SetRangeError(error, String8_Lit("duplicate --length"));
                return Memmy_Status_ParseError;
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &length_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            has_length = 1;
            i++;
        }
    }

    if (!has_start)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("missing --start"));
        return Memmy_Status_ParseError;
    }
    if (has_end && has_length)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("use --end or --length, not both"));
        return Memmy_Status_ParseError;
    }
    if (!has_end && !has_length)
    {
        Memmy_Cli_SetRangeError(error, String8_Lit("missing --end or --length"));
        return Memmy_Status_ParseError;
    }

    Memmy_Addr start = 0;
    Memmy_Status status = Memmy_ParseAddress(start_text, &start, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (has_end)
    {
        Memmy_Addr end = 0;
        status = Memmy_ParseAddress(end_text, &end, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Range_FromStartEnd(start, end, out, error);
    }

    Memmy_Size length = 0;
    status = Memmy_ParseSize(length_text, &length, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return Memmy_Range_FromStartLength(start, length, out, error);
}

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

static void Memmy_Cli_PushLine(Arena *arena, String8List *list, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 line = String8_PushFV(arena, fmt, args);
    va_end(args);
    String8List_Push(arena, list, line);
}

B32 Memmy_Cli_ArgvHasJson(I32 argc, char **argv)
{
    for (I32 i = 1; i < argc; i++)
    {
        if (String8_Eq(String8_FromCStr(argv[i]), String8_Lit("--json")))
        {
            return 1;
        }
    }
    return 0;
}

String8 Memmy_Cli_FormatAddress(Arena *arena, Memmy_PointerWidth pointer_width, Memmy_Addr address)
{
    U32 width = pointer_width == Memmy_PointerWidth_32 ? 8 : 16;
    return String8_PushF(arena, "0x%0*llx", width, (unsigned long long)address);
}

String8 Memmy_Cli_FormatJsonString(Arena *arena, String8 text)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("\""));
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        switch (c)
        {
        case '"':
            String8List_Push(arena, &parts, String8_Lit("\\\""));
            break;
        case '\\':
            String8List_Push(arena, &parts, String8_Lit("\\\\"));
            break;
        case '\b':
            String8List_Push(arena, &parts, String8_Lit("\\b"));
            break;
        case '\f':
            String8List_Push(arena, &parts, String8_Lit("\\f"));
            break;
        case '\n':
            String8List_Push(arena, &parts, String8_Lit("\\n"));
            break;
        case '\r':
            String8List_Push(arena, &parts, String8_Lit("\\r"));
            break;
        case '\t':
            String8List_Push(arena, &parts, String8_Lit("\\t"));
            break;
        default:
            if (c < 0x20)
            {
                String8List_Push(arena, &parts, String8_PushF(arena, "\\u%04x", c));
            }
            else
            {
                String8List_Push(arena, &parts, String8_Make(text.data + i, 1));
            }
            break;
        }
    }
    String8List_Push(arena, &parts, String8_Lit("\""));
    return String8List_Join(arena, &parts, (String8){0});
}

String8 Memmy_Cli_FormatHexBytes(Arena *arena, String8 bytes)
{
    String8List parts = {0};
    for (U64 i = 0; i < bytes.len; i++)
    {
        Memmy_Cli_PushLine(arena, &parts, "%s%02x", i == 0 ? "" : " ", bytes.data[i]);
    }
    return String8List_Join(arena, &parts, (String8){0});
}

String8 Memmy_Cli_FormatJsonError(Arena *arena, Memmy_Error *error)
{
    Memmy_Error fallback = {0};
    if (error == 0)
    {
        error = &fallback;
    }
    String8 status = Memmy_Status_String(error->status);
    String8 status_json = Memmy_Cli_FormatJsonString(arena, status);
    String8 message_json = Memmy_Cli_FormatJsonString(arena, error->message);
    String8 context_json = Memmy_Cli_FormatJsonString(arena, error->context);
    String8 input_json = Memmy_Cli_FormatJsonString(arena, error->input);
    return String8_PushF(arena,
                         "{\"ok\":false,\"error\":{\"status\":%.*s,\"message\":%.*s,\"context\":%.*s,"
                         "\"input\":%.*s,\"byte_offset\":%llu,\"byte_count\":%llu,\"os_code\":%u}}\n",
                         (int)status_json.len, (char *)status_json.data, (int)message_json.len,
                         (char *)message_json.data, (int)context_json.len, (char *)context_json.data,
                         (int)input_json.len, (char *)input_json.data, (unsigned long long)error->byte_offset,
                         (unsigned long long)error->byte_count, error->os_code);
}

static B32 Memmy_Cli_ContainsNoCase(String8 text, String8 needle)
{
    if (needle.len == 0)
    {
        return 1;
    }
    if (needle.len > text.len)
    {
        return 0;
    }

    for (U64 i = 0; i <= text.len - needle.len; i++)
    {
        if (String8_EqNoCase(String8_Substr(text, i, needle.len), needle))
        {
            return 1;
        }
    }
    return 0;
}

static String8 Memmy_Cli_PointerWidthString(Memmy_PointerWidth width)
{
    String8 result = String8_Lit("?");
    if (width == Memmy_PointerWidth_32)
    {
        result = String8_Lit("x86");
    }
    else if (width == Memmy_PointerWidth_64)
    {
        result = String8_Lit("x64");
    }
    return result;
}

static String8 Memmy_Cli_RegionAccessString(Memmy_RegionAccess access)
{
    if (access & Memmy_RegionAccess_Guard)
    {
        return String8_Lit("g--");
    }

    static U8 buffer[4];
    buffer[0] = (access & Memmy_RegionAccess_Read) ? 'r' : '-';
    buffer[1] = (access & Memmy_RegionAccess_Write) ? 'w' : '-';
    buffer[2] = (access & Memmy_RegionAccess_Execute) ? 'x' : '-';
    buffer[3] = 0;
    return String8_Make(buffer, 3);
}

static String8 Memmy_Cli_RegionStateString(Memmy_RegionState state)
{
    String8 result = String8_Lit("free");
    if (state == Memmy_RegionState_Reserved)
    {
        result = String8_Lit("reserved");
    }
    else if (state == Memmy_RegionState_Committed)
    {
        result = String8_Lit("committed");
    }
    return result;
}

static Memmy_Status Memmy_Cli_ParsePid(String8 text, U32 *out, Memmy_Error *error)
{
    Memmy_Size value = 0;
    Memmy_Status status = Memmy_ParseSize(text, &value, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (value > U32_MAX)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("pid"), String8_Lit("pid overflow"));
        return Memmy_Status_Overflow;
    }

    *out = (U32)value;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ParseCount(String8 text, Memmy_Size *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_ParseSize(text, out, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (*out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("count"),
                        String8_Lit("count must be nonzero"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ParseOptions(Arena *arena, I32 argc, char **argv, Memmy_CliOptions *out,
                                           Memmy_Error *error)
{
    *out = (Memmy_CliOptions){0};
    for (I32 i = 1; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, String8_Lit("--help")))
        {
            if (out->help)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --help"), arg);
            }
            out->help = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--version")))
        {
            if (out->version)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --version"), arg);
            }
            out->version = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--json")))
        {
            if (out->json)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --json"), arg);
            }
            out->json = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--jsonl")))
        {
            if (out->jsonl)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --jsonl"), arg);
            }
            out->jsonl = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--pid")))
        {
            if (out->has_pid)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --pid"), arg);
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Cli_ParsePid(value, &out->pid, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_pid = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--name")))
        {
            if (out->has_name)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --name"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->name, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_name = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--filter")))
        {
            if (out->has_filter)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --filter"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->filter, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_filter = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--addr")))
        {
            if (out->has_addr)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --addr"), arg);
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseAddress(value, &out->addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_addr = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--type")))
        {
            if (out->has_type)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --type"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->type_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Type_Parse(out->type_text, &out->type, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_type = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--count")))
        {
            if (out->has_count)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --count"), arg);
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Cli_ParseCount(value, &out->count, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_count = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--value")))
        {
            if (out->has_value)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --value"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValueRaw(argc, argv, i, arg, &out->value_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_value = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--dry-run")))
        {
            if (out->dry_run)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --dry-run"), arg);
            }
            out->dry_run = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--start")))
        {
            if (out->has_start)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --start"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->start_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseAddress(out->start_text, &out->start, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_start = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--end")))
        {
            if (out->has_end)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --end"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->end_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseAddress(out->end_text, &out->end, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_end = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--length")))
        {
            if (out->has_length)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --length"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->length_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseSize(out->length_text, &out->length, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_length = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--limit")))
        {
            if (out->has_limit)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --limit"), arg);
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseSize(value, &out->limit, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_limit = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--chunk-size")))
        {
            if (out->has_chunk_size)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --chunk-size"), arg);
            }
            String8 value = {0};
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_ParseSize(value, &out->chunk_size, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_chunk_size = 1;
            i++;
        }
        else if (String8_Eq(arg, String8_Lit("--pattern")))
        {
            if (out->has_pattern)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --pattern"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValue(argc, argv, i, arg, &out->pattern_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Pattern_Parse(arena, out->pattern_text, Memmy_PatternParseFlag_AllowWildcards, &out->pattern,
                                         error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_pattern = 1;
            i++;
        }
        else if (Memmy_Cli_IsOption(argv[i]))
        {
            return Memmy_Cli_InvalidOption(error, String8_Lit("unknown option"), arg);
        }
        else if (out->command.len == 0)
        {
            out->command = arg;
        }
        else
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("unexpected argument"));
            if (error != 0)
            {
                error->input = arg;
            }
            return Memmy_Status_ParseError;
        }
    }

    if (out->json && out->jsonl)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("use --json or --jsonl, not both"), String8_Lit("--jsonl"));
    }
    if (out->jsonl && !String8_Eq(out->command, String8_Lit("scan")) && !String8_Eq(out->command, String8_Lit("pscan")))
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("--jsonl is only valid for scan and pscan"),
                                       String8_Lit("--jsonl"));
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RequireCap(Memmy_BackendCap cap, Memmy_Error *error)
{
    Memmy_Context *ctx = Memmy_Context_Get();
    if (ctx == 0 || ctx->backend == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"), String8_Lit("missing backend"));
        return Memmy_Status_InvalidArgument;
    }
    if (!Memmy_Backend_HasCapability(ctx->backend, cap))
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("backend"),
                        String8_Lit("backend capability is unavailable"));
        return Memmy_Status_Unsupported;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RequireCaps(Memmy_BackendCap caps, Memmy_Error *error)
{
    Memmy_Context *ctx = Memmy_Context_Get();
    if (ctx == 0 || ctx->backend == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("backend"), String8_Lit("missing backend"));
        return Memmy_Status_InvalidArgument;
    }
    if ((ctx->backend->capabilities & caps) != caps)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("backend"),
                        String8_Lit("backend capability is unavailable"));
        return Memmy_Status_Unsupported;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ResolveTarget(Arena *arena, Memmy_CliOptions *options, U32 *out_pid, Memmy_Error *error)
{
    if (options->has_pid == options->has_name)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("use exactly one target"));
        return Memmy_Status_ParseError;
    }

    if (options->has_pid)
    {
        *out_pid = options->pid;
        return Memmy_Status_Ok;
    }

    Memmy_ProcessList processes = {0};
    Memmy_Status status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U32 match_pid = 0;
    U64 match_count = 0;
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (String8_EqNoCase(info->name, options->name))
        {
            match_pid = info->pid;
            match_count++;
        }
    }

    if (match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("process"), String8_Lit("process was not found"));
        return Memmy_Status_NotFound;
    }
    if (match_count > 1)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("process"),
                        String8_Lit("process name is ambiguous"));
        return Memmy_Status_Ambiguous;
    }

    *out_pid = match_pid;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RejectPokeOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error)
{
    Unused(command);
    if (options->has_value)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("--value is only valid for poke"), String8_Lit("--value"));
    }
    if (options->dry_run)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("--dry-run is only valid for poke"),
                                       String8_Lit("--dry-run"));
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RejectScanOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error)
{
    Unused(command);
    if (options->has_start || options->has_end || options->has_length || options->has_limit ||
        options->has_chunk_size || options->has_pattern)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("scan option is invalid here"), (String8){0});
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ResolveScanRange(Memmy_CliOptions *options, String8 command, Memmy_Range *out,
                                               Memmy_Error *error)
{
    if (!options->has_start)
    {
        String8 message = String8_Eq(command, String8_Lit("pscan")) ? String8_Lit("pscan requires --start")
                                                                    : String8_Lit("scan requires --start");
        Memmy_Error_Set(error, Memmy_Status_ParseError, command, message);
        return Memmy_Status_ParseError;
    }
    if (options->has_end && options->has_length)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, command, String8_Lit("use --end or --length, not both"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_end && !options->has_length)
    {
        String8 message = String8_Eq(command, String8_Lit("pscan")) ? String8_Lit("pscan requires --end or --length")
                                                                    : String8_Lit("scan requires --end or --length");
        Memmy_Error_Set(error, Memmy_Status_ParseError, command, message);
        return Memmy_Status_ParseError;
    }

    if (options->has_end)
    {
        return Memmy_Range_FromStartEnd(options->start, options->end, out, error);
    }
    return Memmy_Range_FromStartLength(options->start, options->length, out, error);
}

static Memmy_Status Memmy_Cli_RejectNonPscanOptions(Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options->has_filter || options->has_addr || options->has_type || options->has_count || options->has_value ||
        options->dry_run)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for pscan"), (String8){0});
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RejectNonScanOptions(Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options->has_filter || options->has_addr || options->has_count || options->dry_run || options->has_pattern)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for scan"), (String8){0});
    }
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_Help(Arena *arena)
{
    return String8_Copy(
        arena,
        String8_Lit("memmy [global-options] <command> [command-options]\n"
                    "\n"
                    "Commands:\n"
                    "  procs\n"
                    "  mods --pid <pid>|--name <name>\n"
                    "  regions --pid <pid>|--name <name>\n"
                    "  peek --pid <pid>|--name <name> --addr <addr> --type <type>\n"
                    "  poke --pid <pid>|--name <name> --addr <addr> --type <type> --value <value>\n"
                    "  scan --pid <pid>|--name <name> --start <addr> --end <addr> --type <type> --value <value>\n"
                    "  scan --pid <pid>|--name <name> --start <addr> --length <size> --type <type> --value <value>\n"
                    "  pscan --pid <pid>|--name <name> --start <addr> --end <addr> --pattern <pattern>\n"
                    "  pscan --pid <pid>|--name <name> --start <addr> --length <size> --pattern <pattern>\n"
                    "\n"
                    "Global options:\n"
                    "  --json\n"
                    "  --jsonl  scan and pscan only\n"
                    "  --help\n"
                    "  --version\n"
                    "\n"
                    "Types: u8 i8 u16 i16 u32 i32 u64 i64 f32 f64 ptr bytes str wstr\n"
                    "Patterns: two-digit hex bytes with ? or ?? wildcards for pscan\n"));
}

static Memmy_Status Memmy_Cli_RunProcs(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListProcs, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("procs"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("procs"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_pid || options->has_name || options->has_addr || options->has_type || options->has_count)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for procs"), (String8){0});
    }

    Memmy_ProcessList processes = {0};
    status = Memmy_ListProcesses(arena, &processes, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    String8List lines = {0};
    if (options->json)
    {
        String8List_Push(arena, &lines, String8_Lit("[\n"));
    }
    else
    {
        String8List_Push(arena, &lines, String8_Lit("PID     ARCH   NAME\n"));
    }
    B32 first_json = 1;
    List_ForEach(Memmy_ProcessInfo, info, &processes.list, link)
    {
        if (options->has_filter && !Memmy_Cli_ContainsNoCase(info->name, options->filter) &&
            !Memmy_Cli_ContainsNoCase(info->path, options->filter))
        {
            continue;
        }

        if (options->json)
        {
            String8 name = Memmy_Cli_FormatJsonString(arena, info->name);
            String8 path = Memmy_Cli_FormatJsonString(arena, info->path);
            U32 pointer_width = info->pointer_width == Memmy_PointerWidth_32 ? 32 : 64;
            Memmy_Cli_PushLine(arena, &lines, "%s  {\"pid\":%u,\"name\":%.*s,\"path\":%.*s,\"pointer_width\":%u}",
                               first_json ? "" : ",\n", info->pid, (int)name.len, (char *)name.data, (int)path.len,
                               (char *)path.data, pointer_width);
            first_json = 0;
        }
        else
        {
            String8 arch = Memmy_Cli_PointerWidthString(info->pointer_width);
            Memmy_Cli_PushLine(arena, &lines, "%u    %.*s    %.*s\n", info->pid, (int)arch.len, (char *)arch.data,
                               (int)info->name.len, (char *)info->name.data);
        }
    }
    if (options->json)
    {
        String8List_Push(arena, &lines, first_json ? String8_Lit("]\n") : String8_Lit("\n]\n"));
    }
    *out = String8List_Join(arena, &lines, (String8){0});
    return Memmy_Status_Ok;
}

static U64 Memmy_Cli_ReadLE(String8 bytes)
{
    U64 result = 0;
    U64 count = Min(bytes.len, 8);
    for (U64 i = 0; i < count; i++)
    {
        result |= ((U64)bytes.data[i]) << (i * 8);
    }
    return result;
}

static I64 Memmy_Cli_ReadSLE(String8 bytes)
{
    U64 unsigned_value = Memmy_Cli_ReadLE(bytes);
    if (bytes.len > 0 && bytes.len < 8 && (bytes.data[bytes.len - 1] & 0x80))
    {
        unsigned_value |= U64_MAX << (bytes.len * 8);
    }
    return (I64)unsigned_value;
}

static Memmy_Status Memmy_Cli_ValidateUtf8(String8 text, Memmy_Error *error)
{
    U64 i = 0;
    while (i < text.len)
    {
        U8 first = text.data[i];
        U32 codepoint = 0;
        U64 need = 0;
        if (first < 0x80)
        {
            codepoint = first;
            need = 1;
        }
        else if (first >= 0xc2 && first <= 0xdf)
        {
            codepoint = first & 0x1f;
            need = 2;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            codepoint = first & 0x0f;
            need = 3;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            codepoint = first & 0x07;
            need = 4;
        }
        else
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("invalid utf-8 leading byte"));
            return Memmy_Status_InvalidEncoding;
        }

        if (i + need > text.len)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("truncated utf-8 sequence"));
            return Memmy_Status_InvalidEncoding;
        }
        for (U64 j = 1; j < need; j++)
        {
            U8 c = text.data[i + j];
            if ((c & 0xc0) != 0x80)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("invalid utf-8 continuation byte"));
                return Memmy_Status_InvalidEncoding;
            }
            codepoint = (codepoint << 6) | (c & 0x3f);
        }
        if ((need == 3 && codepoint < 0x800) || (need == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("invalid utf-8 codepoint"));
            return Memmy_Status_InvalidEncoding;
        }
        i += need;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_ValidateWStr(String8 bytes, Memmy_Error *error)
{
    if ((bytes.len & 1) != 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                        String8_Lit("truncated utf-16 code unit"));
        return Memmy_Status_InvalidEncoding;
    }

    for (U64 i = 0; i < bytes.len; i += 2)
    {
        U16 unit = (U16)(bytes.data[i] | (bytes.data[i + 1] << 8));
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            if (i + 3 >= bytes.len)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("truncated utf-16 surrogate pair"));
                return Memmy_Status_InvalidEncoding;
            }
            U16 low = (U16)(bytes.data[i + 2] | (bytes.data[i + 3] << 8));
            if (low < 0xdc00 || low > 0xdfff)
            {
                Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                                String8_Lit("invalid utf-16 surrogate pair"));
                return Memmy_Status_InvalidEncoding;
            }
            i += 2;
        }
        else if (unit >= 0xdc00 && unit <= 0xdfff)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidEncoding, String8_Lit("peek"),
                            String8_Lit("unpaired utf-16 low surrogate"));
            return Memmy_Status_InvalidEncoding;
        }
    }
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_DecodeWStr(Arena *arena, String8 bytes)
{
    U8 *out = Arena_PushArrayNoZero(arena, U8, bytes.len * 2 + 1);
    U64 at = 0;
    for (U64 i = 0; i < bytes.len; i += 2)
    {
        U32 cp = (U32)(bytes.data[i] | (bytes.data[i + 1] << 8));
        if (cp >= 0xd800 && cp <= 0xdbff)
        {
            U32 low = (U32)(bytes.data[i + 2] | (bytes.data[i + 3] << 8));
            cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
            i += 2;
        }

        if (cp < 0x80)
        {
            out[at++] = (U8)cp;
        }
        else if (cp < 0x800)
        {
            out[at++] = (U8)(0xc0 | (cp >> 6));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
        else if (cp < 0x10000)
        {
            out[at++] = (U8)(0xe0 | (cp >> 12));
            out[at++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
        else
        {
            out[at++] = (U8)(0xf0 | (cp >> 18));
            out[at++] = (U8)(0x80 | ((cp >> 12) & 0x3f));
            out[at++] = (U8)(0x80 | ((cp >> 6) & 0x3f));
            out[at++] = (U8)(0x80 | (cp & 0x3f));
        }
    }
    out[at] = 0;
    return String8_Make(out, at);
}

static String8 Memmy_Cli_EscapeString(Arena *arena, String8 text)
{
    String8List parts = {0};
    String8List_Push(arena, &parts, String8_Lit("\""));
    for (U64 i = 0; i < text.len; i++)
    {
        U8 c = text.data[i];
        switch (c)
        {
        case 0:
            String8List_Push(arena, &parts, String8_Lit("\\0"));
            break;
        case '\n':
            String8List_Push(arena, &parts, String8_Lit("\\n"));
            break;
        case '\r':
            String8List_Push(arena, &parts, String8_Lit("\\r"));
            break;
        case '\t':
            String8List_Push(arena, &parts, String8_Lit("\\t"));
            break;
        case '\\':
            String8List_Push(arena, &parts, String8_Lit("\\\\"));
            break;
        case '"':
            String8List_Push(arena, &parts, String8_Lit("\\\""));
            break;
        default:
            if (c < 0x20 || c == 0x7f)
            {
                String8List_Push(arena, &parts, String8_PushF(arena, "\\x%02x", c));
            }
            else
            {
                String8List_Push(arena, &parts, String8_Make(text.data + i, 1));
            }
            break;
        }
    }
    String8List_Push(arena, &parts, String8_Lit("\""));
    return String8List_Join(arena, &parts, (String8){0});
}

static void Memmy_Cli_PushHexBytes(Arena *arena, String8List *lines, String8 bytes)
{
    for (U64 i = 0; i < bytes.len; i++)
    {
        Memmy_Cli_PushLine(arena, lines, "%s%02x", i == 0 ? "" : " ", bytes.data[i]);
    }
}

static Memmy_Status Memmy_Cli_ResolveReadSize(Memmy_Process *process, Memmy_CliOptions *options, U64 *out_size,
                                              Memmy_Error *error)
{
    Memmy_Type type = options->type;
    if (type.kind == Memmy_TypeKind_Ptr)
    {
        if (options->has_count)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("ptr rejects --count"));
            return Memmy_Status_ParseError;
        }
        if (process->pointer_width == Memmy_PointerWidth_32)
        {
            *out_size = 4;
            return Memmy_Status_Ok;
        }
        if (process->pointer_width == Memmy_PointerWidth_64)
        {
            *out_size = 8;
            return Memmy_Status_Ok;
        }
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("peek"),
                        String8_Lit("unknown process pointer width"));
        return Memmy_Status_InvalidArgument;
    }
    if (type.kind == Memmy_TypeKind_Bytes || type.kind == Memmy_TypeKind_Str || type.kind == Memmy_TypeKind_WStr)
    {
        if (!options->has_count)
        {
            Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"),
                            String8_Lit("variable-width type requires --count"));
            return Memmy_Status_ParseError;
        }
        if (type.kind == Memmy_TypeKind_WStr)
        {
            if (options->count > U64_MAX / 2)
            {
                Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("peek"), String8_Lit("wstr count overflow"));
                return Memmy_Status_Overflow;
            }
            *out_size = options->count * 2;
        }
        else
        {
            *out_size = options->count;
        }
        return Memmy_Status_Ok;
    }
    if (options->has_count)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"),
                        String8_Lit("fixed-width type rejects --count"));
        return Memmy_Status_ParseError;
    }
    *out_size = type.fixed_size;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_FormatPeekValue(Arena *arena, Memmy_CliOptions *options, String8 bytes,
                                              String8List *lines, Memmy_Error *error)
{
    Memmy_Type type = options->type;
    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
    case Memmy_TypeKind_U16:
    case Memmy_TypeKind_U32:
    case Memmy_TypeKind_U64:
    case Memmy_TypeKind_Ptr: {
        U64 value = Memmy_Cli_ReadLE(bytes);
        Memmy_Cli_PushLine(arena, lines, "%llu  0x%0*llx", (unsigned long long)value, (int)(bytes.len * 2),
                           (unsigned long long)value);
    }
    break;
    case Memmy_TypeKind_I8:
    case Memmy_TypeKind_I16:
    case Memmy_TypeKind_I32:
    case Memmy_TypeKind_I64: {
        I64 value = Memmy_Cli_ReadSLE(bytes);
        U64 hex = Memmy_Cli_ReadLE(bytes);
        Memmy_Cli_PushLine(arena, lines, "%lld  0x%0*llx", (long long)value, (int)(bytes.len * 2),
                           (unsigned long long)hex);
    }
    break;
    case Memmy_TypeKind_F32: {
        F32 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        Memmy_Cli_PushLine(arena, lines, "%g", (double)value);
    }
    break;
    case Memmy_TypeKind_F64: {
        F64 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        Memmy_Cli_PushLine(arena, lines, "%g", value);
    }
    break;
    case Memmy_TypeKind_Bytes:
        Memmy_Cli_PushHexBytes(arena, lines, bytes);
        break;
    case Memmy_TypeKind_Str: {
        Memmy_Status status = Memmy_Cli_ValidateUtf8(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 escaped = Memmy_Cli_EscapeString(arena, bytes);
        String8List_Push(arena, lines, escaped);
    }
    break;
    case Memmy_TypeKind_WStr: {
        Memmy_Status status = Memmy_Cli_ValidateWStr(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = Memmy_Cli_DecodeWStr(arena, bytes);
        String8 escaped = Memmy_Cli_EscapeString(arena, text);
        String8List_Push(arena, lines, escaped);
    }
    break;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_FormatJsonValueFields(Arena *arena, Memmy_CliOptions *options, String8 bytes,
                                                    String8 *out, Memmy_Error *error)
{
    Memmy_Type type = options->type;
    switch (type.kind)
    {
    case Memmy_TypeKind_U8:
    case Memmy_TypeKind_U16:
    case Memmy_TypeKind_U32:
    case Memmy_TypeKind_U64:
    case Memmy_TypeKind_Ptr: {
        U64 value = Memmy_Cli_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%llu,\"hex\":\"0x%0*llx\"", (unsigned long long)value,
                             (int)(bytes.len * 2), (unsigned long long)value);
    }
    break;
    case Memmy_TypeKind_I8:
    case Memmy_TypeKind_I16:
    case Memmy_TypeKind_I32:
    case Memmy_TypeKind_I64: {
        I64 value = Memmy_Cli_ReadSLE(bytes);
        U64 hex = Memmy_Cli_ReadLE(bytes);
        *out = String8_PushF(arena, "\"value\":%lld,\"hex\":\"0x%0*llx\"", (long long)value, (int)(bytes.len * 2),
                             (unsigned long long)hex);
    }
    break;
    case Memmy_TypeKind_F32: {
        F32 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        if (isfinite((double)value))
        {
            *out = String8_PushF(arena, "\"value\":%g", (double)value);
        }
        else
        {
            *out = String8_Lit("\"value\":null");
        }
    }
    break;
    case Memmy_TypeKind_F64: {
        F64 value = 0;
        memcpy(&value, bytes.data, sizeof(value));
        if (isfinite(value))
        {
            *out = String8_PushF(arena, "\"value\":%g", value);
        }
        else
        {
            *out = String8_Lit("\"value\":null");
        }
    }
    break;
    case Memmy_TypeKind_Bytes: {
        String8 hex_bytes = Memmy_Cli_FormatHexBytes(arena, bytes);
        String8 value = Memmy_Cli_FormatJsonString(arena, hex_bytes);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    case Memmy_TypeKind_Str: {
        Memmy_Status status = Memmy_Cli_ValidateUtf8(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 value = Memmy_Cli_FormatJsonString(arena, bytes);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    case Memmy_TypeKind_WStr: {
        Memmy_Status status = Memmy_Cli_ValidateWStr(bytes, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 text = Memmy_Cli_DecodeWStr(arena, bytes);
        String8 value = Memmy_Cli_FormatJsonString(arena, text);
        *out = String8_PushF(arena, "\"value\":%.*s", (int)value.len, (char *)value.data);
    }
    break;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RunPeek(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_Read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("peek"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("peek"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_filter)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for peek"), String8_Lit("--filter"));
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!options->has_addr)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("peek requires --addr"));
        return Memmy_Status_ParseError;
    }
    if (!options->has_type)
    {
        Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("peek"), String8_Lit("peek requires --type"));
        return Memmy_Status_ParseError;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Read, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U64 read_size = 0;
    status = Memmy_Cli_ResolveReadSize(process, options, &read_size, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }

    U8 *buffer = Arena_PushArrayNoZero(arena, U8, read_size);
    U64 bytes_read = 0;
    Memmy_PointerWidth pointer_width = process->pointer_width;
    status = Memmy_Process_Read(process, options->addr, buffer, read_size, &bytes_read, error);
    Memmy_Process_Close(process);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != read_size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("peek"), String8_Lit("partial read"));
        return Memmy_Status_PartialRead;
    }

    String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, options->addr);
    if (options->json)
    {
        String8 value_fields = {0};
        status = Memmy_Cli_FormatJsonValueFields(arena, options, String8_Make(buffer, read_size), &value_fields, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8 type_json = Memmy_Cli_FormatJsonString(arena, options->type_text);
        *out =
            String8_PushF(arena, "{\"address\":\"%.*s\",\"type\":%.*s,%.*s}\n", (int)address.len, (char *)address.data,
                          (int)type_json.len, (char *)type_json.data, (int)value_fields.len, (char *)value_fields.data);
    }
    else
    {
        String8List lines = {0};
        Memmy_Cli_PushLine(arena, &lines, "%.*s: %.*s ", (int)address.len, (char *)address.data,
                           (int)options->type_text.len, (char *)options->type_text.data);
        status = Memmy_Cli_FormatPeekValue(arena, options, String8_Make(buffer, read_size), &lines, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        String8List_Push(arena, &lines, String8_Lit("\n"));
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_FormatValue(Arena *arena, Memmy_CliOptions *options, String8 bytes, String8 *out,
                                          Memmy_Error *error)
{
    String8List parts = {0};
    Memmy_Status status = Memmy_Cli_FormatPeekValue(arena, options, bytes, &parts, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    *out = String8List_Join(arena, &parts, (String8){0});
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Cli_RunPoke(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
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

    String8 old_value = {0};
    status = Memmy_Cli_FormatValue(arena, options, String8_Make(old_bytes, size), &old_value, error);
    if (status != Memmy_Status_Ok)
    {
        Memmy_Process_Close(process);
        return status;
    }
    String8 new_display = {0};
    status = Memmy_Cli_FormatValue(arena, options, new_value.bytes, &new_display, error);
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

    String8 address = Memmy_Cli_FormatAddress(arena, pointer_width, options->addr);
    if (options->json)
    {
        String8 type_json = Memmy_Cli_FormatJsonString(arena, options->type_text);
        String8 old_json = Memmy_Cli_FormatJsonString(arena, old_value);
        String8 new_json = Memmy_Cli_FormatJsonString(arena, new_display);
        *out = String8_PushF(arena,
                             "{\"process\":%u,\"address\":\"%.*s\",\"type\":%.*s,\"old\":%.*s,\"new\":%.*s,"
                             "\"dry_run\":%s}\n",
                             pid, (int)address.len, (char *)address.data, (int)type_json.len, (char *)type_json.data,
                             (int)old_json.len, (char *)old_json.data, (int)new_json.len, (char *)new_json.data,
                             options->dry_run ? "true" : "false");
    }
    else
    {
        String8List lines = {0};
        String8 verb = options->dry_run ? String8_Lit("would write") : String8_Lit("wrote");
        Memmy_Cli_PushLine(arena, &lines, "%.*s:\n", (int)verb.len, (char *)verb.data);
        Memmy_Cli_PushLine(arena, &lines, "  process: %u\n", pid);
        Memmy_Cli_PushLine(arena, &lines, "  address: %.*s\n", (int)address.len, (char *)address.data);
        Memmy_Cli_PushLine(arena, &lines, "  type:    %.*s\n", (int)options->type_text.len,
                           (char *)options->type_text.data);
        Memmy_Cli_PushLine(arena, &lines, "  old:     %.*s\n", (int)old_value.len, (char *)old_value.data);
        Memmy_Cli_PushLine(arena, &lines, "  new:     %.*s\n", (int)new_display.len, (char *)new_display.data);
        *out = String8List_Join(arena, &lines, (String8){0});
    }
    return Memmy_Status_Ok;
}

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

static Memmy_Status Memmy_Cli_RunScan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
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

static Memmy_Status Memmy_Cli_RunPscan(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
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

static Memmy_Status Memmy_Cli_RunMods(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListModules, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("mods"), error);
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
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Query, &process, error);
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

static Memmy_Status Memmy_Cli_RunRegions(Arena *arena, Memmy_CliOptions *options, String8 *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Cli_RequireCap(Memmy_BackendCap_ListRegions, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectPokeOptions(options, String8_Lit("regions"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_Cli_RejectScanOptions(options, String8_Lit("regions"), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (options->has_filter || options->has_addr || options->has_type || options->has_count)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for regions"), (String8){0});
    }

    U32 pid = 0;
    status = Memmy_Cli_ResolveTarget(arena, options, &pid, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Process *process = 0;
    status = Memmy_Process_Open(arena, pid, Memmy_ProcessAccess_Query, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_RegionList regions = {0};
    status = Memmy_Process_ListRegions(arena, process, &regions, error);
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
        String8List_Push(arena, &lines,
                         String8_Lit("BASE                END                 SIZE        ACCESS  STATE\n"));
    }
    B32 first_json = 1;
    List_ForEach(Memmy_Region, region, &regions.list, link)
    {
        Memmy_Addr end = 0;
        if (!AddU64Checked(region->base, region->size, &end))
        {
            Memmy_Process_Close(process);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("region"), String8_Lit("region end overflow"));
            return Memmy_Status_Overflow;
        }

        String8 access = Memmy_Cli_RegionAccessString(region->access);
        String8 state = Memmy_Cli_RegionStateString(region->state);
        String8 base = Memmy_Cli_FormatAddress(arena, process->pointer_width, region->base);
        String8 end_text = Memmy_Cli_FormatAddress(arena, process->pointer_width, end);
        if (options->json)
        {
            String8 access_json = Memmy_Cli_FormatJsonString(arena, access);
            String8 state_json = Memmy_Cli_FormatJsonString(arena, state);
            Memmy_Cli_PushLine(arena, &lines,
                               "%s  {\"base\":\"%.*s\",\"end\":\"%.*s\",\"size\":\"0x%llx\","
                               "\"access\":%.*s,\"state\":%.*s}",
                               first_json ? "" : ",\n", (int)base.len, (char *)base.data, (int)end_text.len,
                               (char *)end_text.data, (unsigned long long)region->size, (int)access_json.len,
                               (char *)access_json.data, (int)state_json.len, (char *)state_json.data);
            first_json = 0;
        }
        else
        {
            Memmy_Cli_PushLine(arena, &lines, "%.*s  %.*s  0x%llx     %.*s     %.*s\n", (int)base.len,
                               (char *)base.data, (int)end_text.len, (char *)end_text.data,
                               (unsigned long long)region->size, (int)access.len, (char *)access.data, (int)state.len,
                               (char *)state.data);
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

Memmy_Status Memmy_Cli_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    Memmy_CliOptions options = {0};
    Memmy_Status status = Memmy_Cli_ParseOptions(arena, argc, argv, &options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (options.version)
    {
        *out = String8_Copy(arena, String8_Lit("memmy 0.0.0\n"));
        return Memmy_Status_Ok;
    }
    if (options.help || options.command.len == 0)
    {
        *out = Memmy_Cli_Help(arena);
        return Memmy_Status_Ok;
    }

    if (String8_Eq(options.command, String8_Lit("procs")))
    {
        return Memmy_Cli_RunProcs(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("mods")))
    {
        return Memmy_Cli_RunMods(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("peek")))
    {
        return Memmy_Cli_RunPeek(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("poke")))
    {
        return Memmy_Cli_RunPoke(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("regions")))
    {
        return Memmy_Cli_RunRegions(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("scan")))
    {
        return Memmy_Cli_RunScan(arena, &options, out, error);
    }
    if (String8_Eq(options.command, String8_Lit("pscan")))
    {
        return Memmy_Cli_RunPscan(arena, &options, out, error);
    }

    Memmy_Error_Set(error, Memmy_Status_ParseError, String8_Lit("cli"), String8_Lit("unknown command"));
    if (error != 0)
    {
        error->input = options.command;
    }
    return Memmy_Status_ParseError;
}

I32 Memmy_Cli_ExitCodeFromStatus(Memmy_Status status)
{
    I32 result = 1;
    switch (status)
    {
    case Memmy_Status_Ok:
        result = 0;
        break;
    case Memmy_Status_InvalidArgument:
    case Memmy_Status_ParseError:
    case Memmy_Status_Overflow:
    case Memmy_Status_InvalidEncoding:
        result = 2;
        break;
    case Memmy_Status_NotFound:
    case Memmy_Status_Ambiguous:
        result = 3;
        break;
    case Memmy_Status_AccessDenied:
        result = 4;
        break;
    case Memmy_Status_PartialRead:
    case Memmy_Status_PartialWrite:
        result = 5;
        break;
    case Memmy_Status_Unsupported:
        result = 6;
        break;
    case Memmy_Status_PlatformError:
        result = 7;
        break;
    case Memmy_Status_Unreadable:
    case Memmy_Status_Unwritable:
    case Memmy_Status_OutOfMemory:
        result = 1;
        break;
    }
    return result;
}
