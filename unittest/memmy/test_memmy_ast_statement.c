#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesAssignmentBasics)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstStatement const_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = 42", &const_assignment);
    AssertEq(const_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertStrEq(const_assignment.assignment_name, String8_Lit("foo"));
    AssertEq(const_assignment.assignment_value->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(const_assignment.assignment_value->value, 42);

    Memmy_AstStatement variable_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = $bar", &variable_assignment);
    AssertEq(variable_assignment.assignment_value->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable_assignment.assignment_value->name, String8_Lit("bar"));

    Memmy_AstStatement target_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>", &target_assignment);
    AssertEq(target_assignment.assignment_value->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(target_assignment.assignment_value->target_module, String8_Lit("client.dll"));

    Memmy_AstStatement address_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = @0x1234", &address_assignment);
    AssertEq(address_assignment.assignment_value->kind, Memmy_AstNodeKind_Address);
    AssertEq(address_assignment.assignment_value->value_expr->value, 0x1234);

    Memmy_AstStatement range_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = [@0x1234..@0x5678]", &range_assignment);
    AssertEq(range_assignment.assignment_value->kind, Memmy_AstNodeKind_Range);

    Memmy_AstStatement deref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = $bar->0x42->", &deref_assignment);
    AssertEq(deref_assignment.assignment_value->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_assignment.assignment_value->lhs->kind, Memmy_AstNodeKind_Deref);

    Memmy_AstStatement target_deref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>+0x1234->->0x42->", &target_deref_assignment);
    AssertEq(target_deref_assignment.assignment_value->kind, Memmy_AstNodeKind_Deref);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferencePhase4Assignments)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstStatement pattern_index_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>{ab cd ? ? 12 34}[0]", &pattern_index_assignment);
    AssertEq(pattern_index_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(pattern_index_assignment.assignment_value->kind, Memmy_AstNodeKind_Index);
    AssertEq(pattern_index_assignment.assignment_value->lhs->kind, Memmy_AstNodeKind_PatternScan);

    Memmy_AstStatement value_scan_index_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = (<client.dll> as f32 == 42.777)[2]", &value_scan_index_assignment);
    AssertEq(value_scan_index_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(value_scan_index_assignment.assignment_value->kind, Memmy_AstNodeKind_Index);
    AssertEq(value_scan_index_assignment.assignment_value->lhs->kind, Memmy_AstNodeKind_ValueScan);

    Memmy_AstStatement pattern_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>{aa bb ?? ?? 11 22}", &pattern_assignment);
    AssertEq(pattern_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(pattern_assignment.assignment_value->kind, Memmy_AstNodeKind_PatternScan);
    AssertStrEq(pattern_assignment.assignment_value->pattern, String8_Lit("aa bb ?? ?? 11 22"));

    Memmy_AstStatement ref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = [0..] refs ptr $target", &ref_assignment);
    AssertEq(ref_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(ref_assignment.assignment_value->kind, Memmy_AstNodeKind_ReferenceScan);
    AssertEq(ref_assignment.assignment_value->reference_mode, Memmy_AstReferenceMode_Ptr);

    Memmy_AstNode *ref_index = 0;
    Test_ParseAstExpr(arena, "([0..] refs any @0x1234)[0]", &ref_index);
    AssertEq(ref_index->kind, Memmy_AstNodeKind_Index);
    AssertEq(ref_index->lhs->kind, Memmy_AstNodeKind_ReferenceScan);

    Memmy_AstNode *ref_transform = 0;
    Test_ParseAstExpr(arena, "[0..] refs rel32 @0x1234 => $ + 4", &ref_transform);
    AssertEq(ref_transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(ref_transform->lhs->kind, Memmy_AstNodeKind_ReferenceScan);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceCommands)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstStatement statement = {0};
    Test_ParseAstStatement(arena, "/procs game", &statement);
    AssertEq(statement.kind, Memmy_AstNodeKind_Command);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Procs);
    AssertStrEq(statement.command_arg, String8_Lit("game"));

    Test_ParseAstStatement(arena, "/attach game.exe", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Attach);
    AssertStrEq(statement.command_arg, String8_Lit("game.exe"));

    Test_ParseAstStatement(arena, "/attach 1234", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Attach);
    AssertStrEq(statement.command_arg, String8_Lit("1234"));

    Test_ParseAstStatement(arena, "/detach", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Detach);

    Test_ParseAstStatement(arena, "/mods client", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Mods);
    AssertStrEq(statement.command_arg, String8_Lit("client"));

    Test_ParseAstStatement(arena, "/regions", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Regions);

    Test_ParseAstStatement(arena, "/vars", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Vars);

    Test_ParseAstStatement(arena, "/unset $target_1", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Unset);
    AssertStrEq(statement.command_arg, String8_Lit("target_1"));

    Test_ParseAstStatement(arena, "/clear", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Clear);

    Test_ParseAstStatement(arena, "/help", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Help);

    Test_ParseAstStatement(arena, "/exit", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Exit);

    Test_ParseAstStatement(arena, "/quit", &statement);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Quit);

    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("/procs"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(Memmy_Ast_ParseStatement(arena, String8_Lit("/attach"), &statement, &diagnostic),
             Memmy_AstStatus_ParseError);
    AssertEq(Memmy_Ast_ParseStatement(arena, String8_Lit("/detach now"), &statement, &diagnostic),
             Memmy_AstStatus_ParseError);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_statement =
    TestSuite_Make("Memmy AST Statements", TestCase_Make(Test_MemmyAstParsesAssignmentBasics),
                   TestCase_Make(Test_MemmyAstParsesPocketReferencePhase4Assignments),
                   TestCase_Make(Test_MemmyAstParsesPocketReferenceCommands), );
