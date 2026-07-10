#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesAssignmentBasics)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Statement const_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = 42", &const_assignment);
    AssertEq(const_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertStrEq(const_assignment.assignment_name, String8_Lit("foo"));
    AssertEq(const_assignment.assignment_value->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(const_assignment.assignment_value->value, 42);

    MemmyAst_Statement variable_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = $bar", &variable_assignment);
    AssertEq(variable_assignment.assignment_value->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable_assignment.assignment_value->name, String8_Lit("bar"));

    MemmyAst_Statement target_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>", &target_assignment);
    AssertEq(target_assignment.assignment_value->kind, MemmyAst_NodeKind_Target);
    AssertStrEq(target_assignment.assignment_value->target_module, String8_Lit("client.dll"));

    MemmyAst_Statement address_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = @0x1234", &address_assignment);
    AssertEq(address_assignment.assignment_value->kind, MemmyAst_NodeKind_Address);
    AssertEq(address_assignment.assignment_value->value_expr->value, 0x1234);

    MemmyAst_Statement range_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = [@0x1234..@0x5678]", &range_assignment);
    AssertEq(range_assignment.assignment_value->kind, MemmyAst_NodeKind_Range);

    MemmyAst_Statement deref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = $bar->0x42->", &deref_assignment);
    AssertEq(deref_assignment.assignment_value->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_assignment.assignment_value->lhs->kind, MemmyAst_NodeKind_Deref);

    MemmyAst_Statement target_deref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>+0x1234->->0x42->", &target_deref_assignment);
    AssertEq(target_deref_assignment.assignment_value->kind, MemmyAst_NodeKind_Deref);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferencePhase4Assignments)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Statement pattern_index_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>{ab cd ? ? 12 34}[0]", &pattern_index_assignment);
    AssertEq(pattern_index_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(pattern_index_assignment.assignment_value->kind, MemmyAst_NodeKind_Index);
    AssertEq(pattern_index_assignment.assignment_value->lhs->kind, MemmyAst_NodeKind_PatternScan);

    MemmyAst_Statement value_scan_index_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = (<client.dll> as f32 == 42.777)[2]", &value_scan_index_assignment);
    AssertEq(value_scan_index_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(value_scan_index_assignment.assignment_value->kind, MemmyAst_NodeKind_Index);
    AssertEq(value_scan_index_assignment.assignment_value->lhs->kind, MemmyAst_NodeKind_ValueScan);

    MemmyAst_Statement pattern_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = <client.dll>{aa bb ?? ?? 11 22}", &pattern_assignment);
    AssertEq(pattern_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(pattern_assignment.assignment_value->kind, MemmyAst_NodeKind_PatternScan);
    AssertStrEq(pattern_assignment.assignment_value->pattern, String8_Lit("aa bb ?? ?? 11 22"));

    MemmyAst_Statement ref_assignment = {0};
    Test_ParseAstStatement(arena, "$foo = [0..] refs ptr $target", &ref_assignment);
    AssertEq(ref_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(ref_assignment.assignment_value->kind, MemmyAst_NodeKind_ReferenceScan);
    AssertEq(ref_assignment.assignment_value->reference_mode, MemmyAst_ReferenceMode_Ptr);

    MemmyAst_Node *ref_index = 0;
    Test_ParseAstExpr(arena, "([0..] refs any @0x1234)[0]", &ref_index);
    AssertEq(ref_index->kind, MemmyAst_NodeKind_Index);
    AssertEq(ref_index->lhs->kind, MemmyAst_NodeKind_ReferenceScan);

    MemmyAst_Node *ref_transform = 0;
    Test_ParseAstExpr(arena, "[0..] refs rel32 @0x1234 => $ + 4", &ref_transform);
    AssertEq(ref_transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(ref_transform->lhs->kind, MemmyAst_NodeKind_ReferenceScan);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceCommands)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Statement statement = {0};
    Test_ParseAstStatement(arena, "/procs game", &statement);
    AssertEq(statement.kind, MemmyAst_NodeKind_Command);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Procs);
    AssertStrEq(statement.command_arg, String8_Lit("game"));

    Test_ParseAstStatement(arena, "/attach game.exe", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Attach);
    AssertStrEq(statement.command_arg, String8_Lit("game.exe"));

    Test_ParseAstStatement(arena, "/attach 1234", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Attach);
    AssertStrEq(statement.command_arg, String8_Lit("1234"));

    Test_ParseAstStatement(arena, "/detach", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Detach);

    Test_ParseAstStatement(arena, "/mods client", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Mods);
    AssertStrEq(statement.command_arg, String8_Lit("client"));

    Test_ParseAstStatement(arena, "/regions", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Regions);

    Test_ParseAstStatement(arena, "/vars", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Vars);

    Test_ParseAstStatement(arena, "/unset $target_1", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Unset);
    AssertStrEq(statement.command_arg, String8_Lit("target_1"));

    Test_ParseAstStatement(arena, "/clear", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Clear);

    Test_ParseAstStatement(arena, "/help", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Help);

    Test_ParseAstStatement(arena, "/exit", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Exit);

    Test_ParseAstStatement(arena, "/quit", &statement);
    AssertEq(statement.command_kind, MemmyAst_CommandKind_Quit);

    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("/procs"), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertEq(MemmyAst_Statement_Parse(arena, String8_Lit("/attach"), &statement, &diagnostic),
             MemmyAst_Status_ParseError);
    AssertEq(MemmyAst_Statement_Parse(arena, String8_Lit("/detach now"), &statement, &diagnostic),
             MemmyAst_Status_ParseError);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_statement =
    TestSuite_Make("Memmy AST Statements", TestCase_Make(Test_MemmyAstParsesAssignmentBasics),
                   TestCase_Make(Test_MemmyAstParsesPocketReferencePhase4Assignments),
                   TestCase_Make(Test_MemmyAstParsesPocketReferenceCommands), );
