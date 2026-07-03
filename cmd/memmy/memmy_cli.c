#include "memmy_cli.h"

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
