#include "memmy_ast_parser.h"

static Memmy_AstCommandKind Memmy_Parser_CommandKind(String8 text)
{
    String8 name = String8_Substr(text, 1, text.len - 1);
    if (String8_Eq(name, String8_Lit("procs")))
    {
        return Memmy_AstCommandKind_Procs;
    }
    if (String8_Eq(name, String8_Lit("attach")))
    {
        return Memmy_AstCommandKind_Attach;
    }
    if (String8_Eq(name, String8_Lit("detach")))
    {
        return Memmy_AstCommandKind_Detach;
    }
    if (String8_Eq(name, String8_Lit("mods")))
    {
        return Memmy_AstCommandKind_Mods;
    }
    if (String8_Eq(name, String8_Lit("regions")))
    {
        return Memmy_AstCommandKind_Regions;
    }
    if (String8_Eq(name, String8_Lit("vars")))
    {
        return Memmy_AstCommandKind_Vars;
    }
    if (String8_Eq(name, String8_Lit("unset")))
    {
        return Memmy_AstCommandKind_Unset;
    }
    if (String8_Eq(name, String8_Lit("clear")))
    {
        return Memmy_AstCommandKind_Clear;
    }
    if (String8_Eq(name, String8_Lit("help")))
    {
        return Memmy_AstCommandKind_Help;
    }
    if (String8_Eq(name, String8_Lit("exit")))
    {
        return Memmy_AstCommandKind_Exit;
    }
    if (String8_Eq(name, String8_Lit("quit")))
    {
        return Memmy_AstCommandKind_Quit;
    }
    return Memmy_AstCommandKind_None;
}

static Memmy_AstStatus Memmy_Parser_ParseCommandStatement(Memmy_Parser *parser, Memmy_AstStatement *out)
{
    Memmy_Token command = parser->token;
    Memmy_AstCommandKind kind = Memmy_Parser_CommandKind(command.text);
    if (kind == Memmy_AstCommandKind_None)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unknown command"), command.byte_offset, command.byte_count);
        return Memmy_AstStatus_ParseError;
    }

    Memmy_AstStatus status = Memmy_Parser_Next(parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    out->kind = Memmy_AstNodeKind_Command;
    out->command_kind = kind;
    if (kind == Memmy_AstCommandKind_Procs || kind == Memmy_AstCommandKind_Mods || kind == Memmy_AstCommandKind_Attach)
    {
        U64 start = command.byte_offset + command.byte_count;
        out->command_arg = String8_TrimWhitespace(String8_Substr(parser->input, start, parser->input.len - start));
        if (kind == Memmy_AstCommandKind_Attach && out->command_arg.len == 0)
        {
            Memmy_Parser_SetError(parser, String8_Lit("expected process id or name"), parser->token.byte_offset,
                                  parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }
        parser->pos = parser->input.len;
        return Memmy_Parser_Next(parser);
    }

    if (kind == Memmy_AstCommandKind_Detach)
    {
        if (parser->token.kind != Memmy_TokenKind_Eof)
        {
            Memmy_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                                  parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }
        return Memmy_AstStatus_Ok;
    }

    if (kind == Memmy_AstCommandKind_Unset)
    {
        if (parser->token.kind != Memmy_TokenKind_Variable)
        {
            Memmy_Parser_SetError(parser, String8_Lit("expected variable"), parser->token.byte_offset,
                                  parser->token.byte_count);
            return Memmy_AstStatus_ParseError;
        }
        out->command_arg = String8_Substr(parser->token.text, 1, parser->token.text.len - 1);
        status = Memmy_Parser_Next(parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
    }

    if (parser->token.kind != Memmy_TokenKind_Eof)
    {
        Memmy_Parser_SetError(parser, String8_Lit("unexpected trailing input"), parser->token.byte_offset,
                              parser->token.byte_count);
        return Memmy_AstStatus_ParseError;
    }
    return Memmy_AstStatus_Ok;
}

Memmy_AstStatus Memmy_Ast_ParseStatement(Arena *arena, String8 text, Memmy_AstStatement *out,
                                         Memmy_AstDiagnostic *diagnostic)
{
    if (out != 0)
    {
        *out = (Memmy_AstStatement){0};
    }
    if (diagnostic != 0)
    {
        *diagnostic = (Memmy_AstDiagnostic){0};
    }
    if (arena == 0 || out == 0)
    {
        Memmy_AstDiagnostic_Set(diagnostic, text, String8_Lit("ast"), String8_Lit("missing arena or output"), 0, 0);
        return Memmy_AstStatus_InvalidArgument;
    }

    text = String8_Copy(arena, text);

    Memmy_Parser parser = {
        .arena = arena,
        .input = text,
        .diagnostic = diagnostic,
    };
    Memmy_AstStatus status = Memmy_Parser_Next(&parser);
    if (status != Memmy_AstStatus_Ok)
    {
        return status;
    }

    out->text = text;
    if (parser.token.kind == Memmy_TokenKind_Command)
    {
        status = Memmy_Parser_ParseCommandStatement(&parser, out);
        if (status != Memmy_AstStatus_Ok)
        {
            *out = (Memmy_AstStatement){0};
        }
        return status;
    }

    if (parser.token.kind == Memmy_TokenKind_Variable)
    {
        Memmy_Token variable = parser.token;
        status = Memmy_Parser_Next(&parser);
        if (status != Memmy_AstStatus_Ok)
        {
            return status;
        }
        if (parser.token.kind == Memmy_TokenKind_Equal)
        {
            out->kind = Memmy_AstNodeKind_Assignment;
            out->assignment_name = String8_Substr(variable.text, 1, variable.text.len - 1);
            status = Memmy_Parser_Next(&parser);
            if (status != Memmy_AstStatus_Ok)
            {
                *out = (Memmy_AstStatement){0};
                return status;
            }
            status = Memmy_Parser_ParseExprOnly(&parser, &out->assignment_value);
            if (status != Memmy_AstStatus_Ok)
            {
                *out = (Memmy_AstStatement){0};
                return status;
            }
            return Memmy_AstStatus_Ok;
        }

        parser.pos = variable.byte_offset + variable.byte_count;
        parser.token = variable;
    }

    status = Memmy_Parser_ParseExprOnly(&parser, &out->expr);
    if (status != Memmy_AstStatus_Ok)
    {
        *out = (Memmy_AstStatement){0};
        return status;
    }
    out->kind = out->expr->kind;
    return Memmy_AstStatus_Ok;
}
