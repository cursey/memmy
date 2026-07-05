#include "memmy_cli_internal.h"

#include <stdarg.h>

#include "base_fs.h"

static B32 Memmy_Cli_IsOption(char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-';
}

static Memmy_Status Memmy_Cli_ReadOptionValue(I32 argc, char **argv, I32 index, String8 option, String8 *out,
                                              Memmy_Error *error)
{
    if (index + 1 >= argc || Memmy_Cli_IsOption(argv[index + 1]))
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
    return String8_Eq(option, String8_Lit("--expr"));
}

static B32 Memmy_Cli_OptionConsumesValue(String8 option)
{
    return String8_Eq(option, String8_Lit("--pid")) || String8_Eq(option, String8_Lit("--name"));
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

B32 Memmy_Cli_ArgvHasJsonl(I32 argc, char **argv)
{
    return Memmy_Cli_ArgvHasFormatFlag(argc, argv, String8_Lit("--jsonl"));
}

B32 Memmy_Cli_ArgvHasHelp(I32 argc, char **argv)
{
    return Memmy_Cli_ArgvHasFormatFlag(argc, argv, String8_Lit("--help"));
}

B32 Memmy_Cli_ArgvHasVersion(I32 argc, char **argv)
{
    return Memmy_Cli_ArgvHasFormatFlag(argc, argv, String8_Lit("--version"));
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

String8 Memmy_Cli_FormatJsonlError(Arena *arena, Memmy_Error *error)
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
                         "{\"type\":\"error\",\"status\":%.*s,\"message\":%.*s,\"context\":%.*s,"
                         "\"input\":%.*s,\"byte_offset\":%llu,\"byte_count\":%llu,\"os_code\":%u}\n",
                         (int)status_json.len, (char *)status_json.data, (int)message_json.len,
                         (char *)message_json.data, (int)context_json.len, (char *)context_json.data,
                         (int)input_json.len, (char *)input_json.data, (unsigned long long)error->byte_offset,
                         (unsigned long long)error->byte_count, error->os_code);
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

static Memmy_Status Memmy_Cli_ParseOptions(I32 argc, char **argv, Memmy_CliOptions *out, Memmy_Error *error)
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
        else if (String8_Eq(arg, String8_Lit("--expr")))
        {
            if (out->has_expr)
            {
                return Memmy_Cli_InvalidOption(error, String8_Lit("duplicate --expr"), arg);
            }
            Memmy_Status status = Memmy_Cli_ReadOptionValueRaw(argc, argv, i, arg, &out->expr_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_expr = 1;
            i++;
        }
        else if (Memmy_Cli_IsOption(argv[i]))
        {
            return Memmy_Cli_InvalidOption(error, String8_Lit("unknown option"), arg);
        }
        else if (out->input_path.len == 0)
        {
            out->input_path = arg;
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
    return Memmy_Status_Ok;
}

static String8 Memmy_Cli_Help(Arena *arena)
{
    return String8_Copy(arena, String8_Lit("memmy\n"
                                           "memmy [global-options] [--pid <pid>|--name <name>] --expr <statement>\n"
                                           "memmy [global-options] [--pid <pid>|--name <name>] <file>\n"
                                           "<input> | memmy [global-options] [--pid <pid>|--name <name>]\n"
                                           "\n"
                                           "Run without arguments to start a DSL REPL.\n"
                                           "\n"
                                           "Global options:\n"
                                           "  --jsonl\n"
                                           "  --help\n"
                                           "  --version\n"
                                           "\n"
                                           "Expression options:\n"
                                           "  --expr <statement>\n"
                                           "\n"
                                           "Types: u8 i8 u16 i16 u32 i32 u64 i64 f32 f64 ptr bytes str wstr\n"
                                           "Patterns: two-digit hex bytes with ? or ?? wildcards\n"));
}

static Memmy_Status Memmy_Cli_UseOneInputSource(Memmy_Error *error, String8 input)
{
    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"),
                    String8_Lit("use exactly one input source"));
    if (error != 0)
    {
        error->input = input;
    }
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Cli_RunScriptString(Arena *arena, Memmy_CliOptions *options, String8 input, String8 *out,
                                              Memmy_Error *error)
{
    return Memmy_Cli_RunReplStringWithOptions(arena, options, input, out, error);
}

typedef struct Memmy_CliStringWriter Memmy_CliStringWriter;
struct Memmy_CliStringWriter
{
    Arena *arena;
    String8List list;
};

static Memmy_Status Memmy_CliStringWriter_Write(void *user_data, String8 text)
{
    Memmy_CliStringWriter *writer = (Memmy_CliStringWriter *)user_data;
    String8List_Push(writer->arena, &writer->list, text);
    return Memmy_Status_Ok;
}

static Memmy_CliOutputWriter Memmy_CliStringWriter_Make(Memmy_CliStringWriter *writer, Arena *arena)
{
    *writer = (Memmy_CliStringWriter){
        .arena = arena,
    };
    return (Memmy_CliOutputWriter){
        .write = Memmy_CliStringWriter_Write,
        .user_data = writer,
    };
}

Memmy_Status Memmy_Cli_RunInputString(Arena *arena, I32 argc, char **argv, String8 input, String8 *out,
                                      Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    Memmy_CliOptions options = {0};
    Memmy_Status status = Memmy_Cli_ParseOptions(argc, argv, &options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (options.version)
    {
        *out = String8_Copy(arena, String8_Lit("memmy 0.0.0\n"));
        return Memmy_Status_Ok;
    }
    if (options.help)
    {
        *out = Memmy_Cli_Help(arena);
        return Memmy_Status_Ok;
    }
    if (options.has_expr)
    {
        return Memmy_Cli_UseOneInputSource(error, String8_Lit("--expr"));
    }
    if (options.input_path.len != 0)
    {
        return Memmy_Cli_UseOneInputSource(error, options.input_path);
    }

    return Memmy_Cli_RunScriptString(arena, &options, input, out, error);
}

Memmy_Status Memmy_Cli_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    Memmy_CliStringWriter string_writer = {0};
    Memmy_CliOutputWriter writer = Memmy_CliStringWriter_Make(&string_writer, arena);
    Memmy_Status status = Memmy_Cli_RunToWriter(arena, argc, argv, writer, error);
    *out = String8List_Join(arena, &string_writer.list, (String8){0});
    return status;
}

Memmy_Status Memmy_Cli_RunToWriter(Arena *arena, I32 argc, char **argv, Memmy_CliOutputWriter writer,
                                   Memmy_Error *error)
{
    if (arena == 0 || writer.write == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output writer"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_CliOptions options = {0};
    Memmy_Status status = Memmy_Cli_ParseOptions(argc, argv, &options, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (options.version)
    {
        return writer.write(writer.user_data, String8_Lit("memmy 0.0.0\n"));
    }
    if (options.help)
    {
        String8 help = Memmy_Cli_Help(arena);
        return writer.write(writer.user_data, help);
    }
    if (options.has_expr)
    {
        if (options.input_path.len != 0)
        {
            return Memmy_Cli_UseOneInputSource(error, String8_Lit("--expr"));
        }
        return Memmy_Cli_RunExprToWriter(arena, &options, writer, error);
    }
    if (options.input_path.len == 0)
    {
        String8 help = Memmy_Cli_Help(arena);
        return writer.write(writer.user_data, help);
    }

    String8 input = {0};
    if (!Fs_ReadFile(arena, options.input_path, &input))
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("file"), String8_Lit("failed to read input file"));
        if (error != 0)
        {
            error->input = options.input_path;
        }
        return Memmy_Status_NotFound;
    }

    String8 output = {0};
    status = Memmy_Cli_RunScriptString(arena, &options, input, &output, error);
    if (output.len > 0)
    {
        Memmy_Status write_status = writer.write(writer.user_data, output);
        if (write_status != Memmy_Status_Ok)
        {
            return write_status;
        }
    }
    return status;
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
