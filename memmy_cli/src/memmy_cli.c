#include "memmy_cli_internal.h"

#include <stdarg.h>

#include "base.h"

static B32 MemmyCli_Option_Is(char *arg)
{
    return arg != 0 && arg[0] == '-' && arg[1] == '-';
}

static Memmy_Status MemmyCli_Option_ReadValue(I32 argc, char **argv, I32 index, String8 option, String8 *out,
                                              Memmy_Error *error)
{
    if (index + 1 >= argc || MemmyCli_Option_Is(argv[index + 1]))
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

static Memmy_Status MemmyCli_Option_ReadRawValue(I32 argc, char **argv, I32 index, String8 option, String8 *out,
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

Memmy_Status MemmyCli_Option_Invalid(Memmy_Error *error, String8 message, String8 input)
{
    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), message);
    if (error != 0)
    {
        error->input = input;
    }
    return Memmy_Status_InvalidArgument;
}

void MemmyCli_Line_Push(Arena *arena, String8List *list, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String8 line = String8_PushFV(arena, fmt, args);
    va_end(args);
    String8List_Push(arena, list, line);
}

static B32 MemmyCli_Option_ConsumesRawValue(String8 option)
{
    return String8_Eq(option, String8_Lit("--expr"));
}

static B32 MemmyCli_Option_ConsumesValue(String8 option)
{
    return String8_Eq(option, String8_Lit("--pid")) || String8_Eq(option, String8_Lit("--name"));
}

static B32 MemmyCli_Argv_HasFormatFlag(I32 argc, char **argv, String8 flag)
{
    for (I32 i = 1; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, flag))
        {
            return 1;
        }
        if (MemmyCli_Option_ConsumesRawValue(arg) && i + 1 < argc)
        {
            i++;
        }
        else if (MemmyCli_Option_ConsumesValue(arg) && i + 1 < argc && !MemmyCli_Option_Is(argv[i + 1]))
        {
            i++;
        }
    }
    return 0;
}

B32 MemmyCli_Argv_HasJsonl(I32 argc, char **argv)
{
    return MemmyCli_Argv_HasFormatFlag(argc, argv, String8_Lit("--jsonl"));
}

B32 MemmyCli_Argv_HasHelp(I32 argc, char **argv)
{
    return MemmyCli_Argv_HasFormatFlag(argc, argv, String8_Lit("--help"));
}

B32 MemmyCli_Argv_HasVersion(I32 argc, char **argv)
{
    return MemmyCli_Argv_HasFormatFlag(argc, argv, String8_Lit("--version"));
}

String8 MemmyCli_Address_Format(Arena *arena, Memmy_PointerWidth pointer_width, Memmy_Addr address)
{
    U32 width = pointer_width == Memmy_PointerWidth_32 ? 8 : 16;
    return String8_PushF(arena, "0x%0*llx", width, (unsigned long long)address);
}

String8 MemmyCli_JsonString_Format(Arena *arena, String8 text)
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

String8 MemmyCli_JsonlError_Format(Arena *arena, Memmy_Error *error)
{
    Memmy_Error fallback = {0};
    if (error == 0)
    {
        error = &fallback;
    }
    String8 status = Memmy_Status_String(error->status);
    String8 status_json = MemmyCli_JsonString_Format(arena, status);
    String8 message_json = MemmyCli_JsonString_Format(arena, error->message);
    String8 context_json = MemmyCli_JsonString_Format(arena, error->context);
    String8 input_json = MemmyCli_JsonString_Format(arena, error->input);
    return String8_PushF(arena,
                         "{\"type\":\"error\",\"status\":%.*s,\"message\":%.*s,\"context\":%.*s,"
                         "\"input\":%.*s,\"byte_offset\":%llu,\"byte_count\":%llu,\"os_code\":%u}\n",
                         (int)status_json.len, (char *)status_json.data, (int)message_json.len,
                         (char *)message_json.data, (int)context_json.len, (char *)context_json.data,
                         (int)input_json.len, (char *)input_json.data, (unsigned long long)error->byte_offset,
                         (unsigned long long)error->byte_count, error->os_code);
}

static Memmy_Status MemmyCli_Pid_Parse(String8 text, U32 *out, Memmy_Error *error)
{
    Memmy_Size value = 0;
    Memmy_Status status = Memmy_Size_Parse(text, &value, error);
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

static Memmy_Status MemmyCli_Options_Parse(I32 argc, char **argv, MemmyCli_Options *out, Memmy_Error *error)
{
    *out = (MemmyCli_Options){0};
    for (I32 i = 1; i < argc; i++)
    {
        String8 arg = String8_FromCStr(argv[i]);
        if (String8_Eq(arg, String8_Lit("--help")))
        {
            if (out->help)
            {
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --help"), arg);
            }
            out->help = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--version")))
        {
            if (out->version)
            {
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --version"), arg);
            }
            out->version = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--jsonl")))
        {
            if (out->jsonl)
            {
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --jsonl"), arg);
            }
            out->jsonl = 1;
        }
        else if (String8_Eq(arg, String8_Lit("--pid")))
        {
            if (out->has_pid)
            {
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --pid"), arg);
            }
            String8 value = {0};
            Memmy_Status status = MemmyCli_Option_ReadValue(argc, argv, i, arg, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = MemmyCli_Pid_Parse(value, &out->pid, error);
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
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --name"), arg);
            }
            Memmy_Status status = MemmyCli_Option_ReadValue(argc, argv, i, arg, &out->name, error);
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
                return MemmyCli_Option_Invalid(error, String8_Lit("duplicate --expr"), arg);
            }
            Memmy_Status status = MemmyCli_Option_ReadRawValue(argc, argv, i, arg, &out->expr_text, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            out->has_expr = 1;
            i++;
        }
        else if (MemmyCli_Option_Is(argv[i]))
        {
            return MemmyCli_Option_Invalid(error, String8_Lit("unknown option"), arg);
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

static String8 MemmyCli_Argv_Help(Arena *arena)
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
                                           "Types: u8 i8 u16 i16 u32 i32 u64 i64 f32 f64 str wstr\n"
                                           "Patterns: two-digit hex bytes with ? or ?? wildcards\n"));
}

static Memmy_Status MemmyCli_InputSource_RequireOne(Memmy_Error *error, String8 input)
{
    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"),
                    String8_Lit("use exactly one input source"));
    if (error != 0)
    {
        error->input = input;
    }
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status MemmyCli_Script_RunString(Arena *arena, MemmyCli_Options *options, String8 input, String8 *out,
                                              Memmy_Error *error)
{
    return MemmyCli_Repl_RunStringWithOptions(arena, options, input, out, error);
}

typedef struct MemmyCli_StringWriter MemmyCli_StringWriter;
struct MemmyCli_StringWriter
{
    Arena *arena;
    String8List list;
};

static Memmy_Status MemmyCli_StringWriter_Write(void *user_data, String8 text)
{
    MemmyCli_StringWriter *writer = (MemmyCli_StringWriter *)user_data;
    String8List_Push(writer->arena, &writer->list, text);
    return Memmy_Status_Ok;
}

static MemmyCli_OutputWriter MemmyCli_StringWriter_Make(MemmyCli_StringWriter *writer, Arena *arena)
{
    *writer = (MemmyCli_StringWriter){
        .arena = arena,
    };
    return (MemmyCli_OutputWriter){
        .write = MemmyCli_StringWriter_Write,
        .user_data = writer,
    };
}

Memmy_Status MemmyCli_Input_RunString(Arena *arena, I32 argc, char **argv, String8 input, String8 *out,
                                      Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    MemmyCli_Options options = {0};
    Memmy_Status status = MemmyCli_Options_Parse(argc, argv, &options, error);
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
        *out = MemmyCli_Argv_Help(arena);
        return Memmy_Status_Ok;
    }
    if (options.has_expr)
    {
        return MemmyCli_InputSource_RequireOne(error, String8_Lit("--expr"));
    }
    if (options.input_path.len != 0)
    {
        return MemmyCli_InputSource_RequireOne(error, options.input_path);
    }

    return MemmyCli_Script_RunString(arena, &options, input, out, error);
}

Memmy_Status MemmyCli_Argv_RunToString(Arena *arena, I32 argc, char **argv, String8 *out, Memmy_Error *error)
{
    if (arena == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    MemmyCli_StringWriter string_writer = {0};
    MemmyCli_OutputWriter writer = MemmyCli_StringWriter_Make(&string_writer, arena);
    Memmy_Status status = MemmyCli_Argv_RunToWriter(arena, argc, argv, writer, error);
    *out = String8List_Join(arena, &string_writer.list, (String8){0});
    return status;
}

Memmy_Status MemmyCli_Argv_RunToWriter(Arena *arena, I32 argc, char **argv, MemmyCli_OutputWriter writer,
                                       Memmy_Error *error)
{
    if (arena == 0 || writer.write == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("cli"), String8_Lit("missing output writer"));
        return Memmy_Status_InvalidArgument;
    }

    MemmyCli_Options options = {0};
    Memmy_Status status = MemmyCli_Options_Parse(argc, argv, &options, error);
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
        String8 help = MemmyCli_Argv_Help(arena);
        return writer.write(writer.user_data, help);
    }
    if (options.has_expr)
    {
        if (options.input_path.len != 0)
        {
            return MemmyCli_InputSource_RequireOne(error, String8_Lit("--expr"));
        }
        return MemmyCli_Expr_RunToWriter(arena, &options, writer, error);
    }
    if (options.input_path.len == 0)
    {
        String8 help = MemmyCli_Argv_Help(arena);
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
    status = MemmyCli_Script_RunString(arena, &options, input, &output, error);
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

I32 MemmyCli_ExitCode_FromStatus(Memmy_Status status)
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
