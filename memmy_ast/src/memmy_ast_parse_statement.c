#include "memmy_ast_parser.h"

static MemmyAst_CommandKind MemmyAst_Parser_CommandKind(String8 text)
{
    String8 name = String8_Substr(text, 1, text.len - 1);
    if (String8_Eq(name, String8_Lit("procs")))
    {
        return MemmyAst_CommandKind_Procs;
    }
    if (String8_Eq(name, String8_Lit("attach")))
    {
        return MemmyAst_CommandKind_Attach;
    }
    if (String8_Eq(name, String8_Lit("detach")))
    {
        return MemmyAst_CommandKind_Detach;
    }
    if (String8_Eq(name, String8_Lit("mods")))
    {
        return MemmyAst_CommandKind_Mods;
    }
    if (String8_Eq(name, String8_Lit("regions")))
    {
        return MemmyAst_CommandKind_Regions;
    }
    if (String8_Eq(name, String8_Lit("vars")))
    {
        return MemmyAst_CommandKind_Vars;
    }
    if (String8_Eq(name, String8_Lit("unset")))
    {
        return MemmyAst_CommandKind_Unset;
    }
    if (String8_Eq(name, String8_Lit("clear")))
    {
        return MemmyAst_CommandKind_Clear;
    }
    if (String8_Eq(name, String8_Lit("help")))
    {
        return MemmyAst_CommandKind_Help;
    }
    if (String8_Eq(name, String8_Lit("tutorial")))
    {
        return MemmyAst_CommandKind_Tutorial;
    }
    if (String8_Eq(name, String8_Lit("exit")))
    {
        return MemmyAst_CommandKind_Exit;
    }
    if (String8_Eq(name, String8_Lit("quit")))
    {
        return MemmyAst_CommandKind_Quit;
    }
    return MemmyAst_CommandKind_None;
}

static MemmyAst_Status MemmyAst_Parser_ParseCommandStatement(MemmyAst_Parser *parser, MemmyAst_Statement *out)
{
    MemmyAst_Token command = parser->token;
    MemmyAst_CommandKind kind = MemmyAst_Parser_CommandKind(command.text);
    if (kind == MemmyAst_CommandKind_None)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unknown command"), command.byte_offset, command.byte_count);
        return MemmyAst_Status_ParseError;
    }

    MemmyAst_Status status = MemmyAst_Parser_Next(parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    out->kind = MemmyAst_NodeKind_Command;
    out->command_kind = kind;
    if (kind == MemmyAst_CommandKind_Procs || kind == MemmyAst_CommandKind_Mods ||
        kind == MemmyAst_CommandKind_Attach || kind == MemmyAst_CommandKind_Tutorial)
    {
        U64 start = command.byte_offset + command.byte_count;
        out->command_arg = String8_TrimWhitespace(String8_Substr(parser->input, start, parser->input.len - start));
        if (kind == MemmyAst_CommandKind_Attach && out->command_arg.len == 0)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("expected process id or name"), parser->token.byte_offset,
                                     parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }
        parser->pos = parser->input.len;
        return MemmyAst_Parser_Next(parser);
    }

    if (kind == MemmyAst_CommandKind_Detach)
    {
        if (parser->token.kind != MemmyAst_TokenKind_Eof)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                                     parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }
        return MemmyAst_Status_Ok;
    }

    if (kind == MemmyAst_CommandKind_Unset)
    {
        if (parser->token.kind != MemmyAst_TokenKind_Variable)
        {
            MemmyAst_Parser_SetError(parser, String8_Lit("expected variable"), parser->token.byte_offset,
                                     parser->token.byte_count);
            return MemmyAst_Status_ParseError;
        }
        out->command_arg = String8_Substr(parser->token.text, 1, parser->token.text.len - 1);
        status = MemmyAst_Parser_Next(parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
    }

    if (parser->token.kind != MemmyAst_TokenKind_Eof)
    {
        MemmyAst_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                                 parser->token.byte_count);
        return MemmyAst_Status_ParseError;
    }
    return MemmyAst_Status_Ok;
}

MemmyAst_Status MemmyAst_Statement_Parse(Arena *arena, String8 text, MemmyAst_Statement *out,
                                         MemmyAst_Diagnostic *diagnostic)
{
    if (out != 0)
    {
        *out = (MemmyAst_Statement){0};
    }
    if (diagnostic != 0)
    {
        *diagnostic = (MemmyAst_Diagnostic){0};
    }
    if (arena == 0 || out == 0)
    {
        MemmyAst_Diagnostic_Set(diagnostic, text, String8_Lit("ast"), String8_Lit("missing arena or output"), 0, 0);
        return MemmyAst_Status_InvalidArgument;
    }

    text = String8_Copy(arena, text);

    MemmyAst_Parser parser = {
        .arena = arena,
        .input = text,
        .diagnostic = diagnostic,
    };
    MemmyAst_Status status = MemmyAst_Parser_Next(&parser);
    if (status != MemmyAst_Status_Ok)
    {
        return status;
    }

    out->text = text;
    if (parser.token.kind == MemmyAst_TokenKind_Command)
    {
        status = MemmyAst_Parser_ParseCommandStatement(&parser, out);
        if (status != MemmyAst_Status_Ok)
        {
            *out = (MemmyAst_Statement){0};
        }
        return status;
    }

    if (parser.token.kind == MemmyAst_TokenKind_Variable)
    {
        MemmyAst_Token variable = parser.token;
        status = MemmyAst_Parser_Next(&parser);
        if (status != MemmyAst_Status_Ok)
        {
            return status;
        }
        if (parser.token.kind == MemmyAst_TokenKind_Equal)
        {
            out->kind = MemmyAst_NodeKind_Assignment;
            out->assignment_name = String8_Substr(variable.text, 1, variable.text.len - 1);
            status = MemmyAst_Parser_Next(&parser);
            if (status != MemmyAst_Status_Ok)
            {
                *out = (MemmyAst_Statement){0};
                return status;
            }
            status = MemmyAst_Parser_ParseExprOnly(&parser, &out->assignment_value);
            if (status != MemmyAst_Status_Ok)
            {
                *out = (MemmyAst_Statement){0};
                return status;
            }
            return MemmyAst_Status_Ok;
        }

        parser.pos = variable.byte_offset + variable.byte_count;
        parser.token = variable;
    }

    status = MemmyAst_Parser_ParseExprOnly(&parser, &out->expr);
    if (status != MemmyAst_Status_Ok)
    {
        *out = (MemmyAst_Statement){0};
        return status;
    }
    out->kind = out->expr->kind;
    return MemmyAst_Status_Ok;
}
