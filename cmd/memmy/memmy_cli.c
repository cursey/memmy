#include "memmy_cli_internal.h"

#include <stdarg.h>

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

Memmy_Status Memmy_Cli_InvalidOption(Memmy_Error *error, String8 message, String8 input)
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

void Memmy_Cli_PushLine(Arena *arena, String8List *list, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 line = String8_PushFV(arena, fmt, args);
    va_end(args);
    String8List_Push(arena, list, line);
}

static B32 Memmy_Cli_OptionConsumesRawValue(String8 option)
{
    return String8_Eq(option, String8_Lit("--value"));
}

static B32 Memmy_Cli_OptionConsumesValue(String8 option)
{
    return String8_Eq(option, String8_Lit("--pid")) || String8_Eq(option, String8_Lit("--name")) ||
           String8_Eq(option, String8_Lit("--filter")) || String8_Eq(option, String8_Lit("--addr")) ||
           String8_Eq(option, String8_Lit("--type")) || String8_Eq(option, String8_Lit("--count")) ||
           String8_Eq(option, String8_Lit("--start")) || String8_Eq(option, String8_Lit("--end")) ||
           String8_Eq(option, String8_Lit("--length")) || String8_Eq(option, String8_Lit("--limit")) ||
           String8_Eq(option, String8_Lit("--chunk-size")) || String8_Eq(option, String8_Lit("--pattern"));
}

static B32 Memmy_Cli_ArgvHasFormatFlag(I32 argc, char **argv, String8 flag)
{
    for (I32 i = 1; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, flag))
        {
            return 1;
        }
        if (Memmy_Cli_OptionConsumesRawValue(arg) && i + 1 < argc)
        {
            i++;
        }
        else if (Memmy_Cli_OptionConsumesValue(arg) && i + 1 < argc && !Memmy_Cli_IsOption(argv[i + 1]))
        {
            i++;
        }
    }
    return 0;
}

B32 Memmy_Cli_ArgvHasJson(I32 argc, char **argv)
{
    return Memmy_Cli_ArgvHasFormatFlag(argc, argv, String8_Lit("--json"));
}

B32 Memmy_Cli_ArgvHasJsonl(I32 argc, char **argv)
{
    return Memmy_Cli_ArgvHasFormatFlag(argc, argv, String8_Lit("--jsonl"));
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

String8 Memmy_Cli_FormatJsonlError(Arena *arena, Memmy_Error *error)
{
    return Memmy_Cli_FormatJsonError(arena, error);
}

B32 Memmy_Cli_ContainsNoCase(String8 text, String8 needle)
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

String8 Memmy_Cli_PointerWidthString(Memmy_PointerWidth width)
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

String8 Memmy_Cli_RegionAccessString(Memmy_RegionAccess access)
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

String8 Memmy_Cli_RegionStateString(Memmy_RegionState state)
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

Memmy_Status Memmy_Cli_RequireCap(Memmy_BackendCap cap, Memmy_Error *error)
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

Memmy_Status Memmy_Cli_RequireCaps(Memmy_BackendCap caps, Memmy_Error *error)
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

Memmy_Status Memmy_Cli_ResolveTarget(Arena *arena, Memmy_CliOptions *options, U32 *out_pid, Memmy_Error *error)
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

Memmy_Status Memmy_Cli_RejectPokeOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error)
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

Memmy_Status Memmy_Cli_RejectScanOptions(Memmy_CliOptions *options, String8 command, Memmy_Error *error)
{
    Unused(command);
    if (options->has_start || options->has_end || options->has_length || options->has_limit ||
        options->has_chunk_size || options->has_pattern)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("scan option is invalid here"), (String8){0});
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_ResolveScanRange(Memmy_CliOptions *options, String8 command, Memmy_Range *out,
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

Memmy_Status Memmy_Cli_RejectNonPscanOptions(Memmy_CliOptions *options, Memmy_Error *error)
{
    if (options->has_filter || options->has_addr || options->has_type || options->has_count || options->has_value ||
        options->dry_run)
    {
        return Memmy_Cli_InvalidOption(error, String8_Lit("option is invalid for pscan"), (String8){0});
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Cli_RejectNonScanOptions(Memmy_CliOptions *options, Memmy_Error *error)
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
