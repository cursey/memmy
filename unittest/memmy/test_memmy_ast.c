#include "memmy_ast.h"
#include "test_framework.h"

static void Test_ParseAstExpr(Arena *arena, char *text, Memmy_AstNode **out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
    AssertTrue(*out != 0);
}

static void Test_ParseAstStatement(Arena *arena, char *text, Memmy_AstStatement *out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseStatement(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
}

static void Test_RejectAstExpr(char *text)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertTrue(expr == 0);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesConstantsWithPrecedence)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseAstExpr(arena, "0x1234 - 32 * (4 + 5)", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(expr->op, Memmy_AstConstOp_Sub);
    AssertEq(expr->value, 0x1234 - 32 * (4 + 5));
    AssertEq(expr->lhs->value, 0x1234);
    AssertEq(expr->rhs->op, Memmy_AstConstOp_Mul);
    AssertEq(expr->rhs->rhs->op, Memmy_AstConstOp_Add);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesVariablesInConstExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseAstExpr(arena, "$foo + 7 * $bar", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertTrue(expr->contains_variable);
    AssertEq(expr->op, Memmy_AstConstOp_Add);
    AssertEq(expr->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(expr->lhs->name, String8_Lit("foo"));
    AssertEq(expr->rhs->rhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(expr->rhs->rhs->name, String8_Lit("bar"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesParenthesizedTypedReadsInArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseAstExpr(arena, "$entity_list_mov + 7 + ($entity_list_mov + 3 as i32)", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(expr->op, Memmy_AstConstOp_Add);
    AssertEq(expr->rhs->kind, Memmy_AstNodeKind_TypedRead);
    AssertStrEq(expr->rhs->type_name, String8_Lit("i32"));
    AssertEq(expr->rhs->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(expr->rhs->lhs->op, Memmy_AstConstOp_Add);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsInvalidIdentifiers)
{
    String8 rejected[] = {
        String8_Lit("$"),
        String8_Lit("$1"),
        String8_Lit("$foo-bar"),
        String8_Lit("client.dll"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_AstNode *expr = 0;
        Memmy_AstDiagnostic diagnostic = {0};
        AssertEq(Memmy_Ast_ParseExpr(arena, rejected[i], &expr, &diagnostic), Memmy_AstStatus_ParseError);
        AssertTrue(expr == 0);
        AssertStrEq(diagnostic.context, String8_Lit("ast"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyAstParsesPocketReferenceTargets)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *process_name = 0;
    Test_ParseAstExpr(arena, "<game.exe!>", &process_name);
    AssertEq(process_name->kind, Memmy_AstNodeKind_Target);
    AssertTrue(process_name->target_has_process);
    AssertTrue(!process_name->target_process_is_pid);
    AssertStrEq(process_name->target_process, String8_Lit("game.exe"));
    AssertStrEq(process_name->target_module, String8_Lit(""));

    Memmy_AstNode *process_pid = 0;
    Test_ParseAstExpr(arena, "<1234!>", &process_pid);
    AssertTrue(process_pid->target_has_process);
    AssertTrue(process_pid->target_process_is_pid);
    AssertStrEq(process_pid->target_process, String8_Lit("1234"));

    Memmy_AstNode *module = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module);
    AssertTrue(!module->target_has_process);
    AssertStrEq(module->target_module, String8_Lit("client.dll"));

    Memmy_AstNode *process_module_name = 0;
    Test_ParseAstExpr(arena, "<game.exe!client.dll>", &process_module_name);
    AssertTrue(process_module_name->target_has_process);
    AssertStrEq(process_module_name->target_process, String8_Lit("game.exe"));
    AssertStrEq(process_module_name->target_module, String8_Lit("client.dll"));

    Memmy_AstNode *process_module_pid = 0;
    Test_ParseAstExpr(arena, "<1234!client.dll>", &process_module_pid);
    AssertTrue(process_module_pid->target_has_process);
    AssertTrue(process_module_pid->target_process_is_pid);
    AssertStrEq(process_module_pid->target_process, String8_Lit("1234"));
    AssertStrEq(process_module_pid->target_module, String8_Lit("client.dll"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceCoreValues)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *constant = 0;
    Test_ParseAstExpr(arena, "32 * (4 + 5)", &constant);
    AssertEq(constant->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(constant->value, 32 * (4 + 5));

    Memmy_AstNode *address = 0;
    Test_ParseAstExpr(arena, "@0x1234", &address);
    AssertEq(address->kind, Memmy_AstNodeKind_Address);
    AssertEq(address->value_expr->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(address->value_expr->value, 0x1234);

    Memmy_AstNode *explicit_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]", &explicit_range);
    AssertEq(explicit_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(!explicit_range->range_is_sized);
    AssertEq(explicit_range->lhs->kind, Memmy_AstNodeKind_Address);
    AssertEq(explicit_range->rhs->kind, Memmy_AstNodeKind_Address);

    Memmy_AstNode *sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..+0x5678]", &sized_range);
    AssertEq(sized_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(sized_range->range_is_sized);
    AssertEq(sized_range->lhs->kind, Memmy_AstNodeKind_Address);
    AssertEq(sized_range->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(sized_range->rhs->value, 0x5678);

    Memmy_AstNode *target = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &target);
    AssertEq(target->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable = 0;
    Test_ParseAstExpr(arena, "$name", &variable);
    AssertEq(variable->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("name"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceRanges)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *explicit_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]", &explicit_range);
    AssertEq(explicit_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(!explicit_range->range_is_sized);
    AssertEq(explicit_range->lhs->value_expr->value, 0x1234);
    AssertEq(explicit_range->rhs->value_expr->value, 0x5678);

    Memmy_AstNode *sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..+0x5678]", &sized_range);
    AssertEq(sized_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(sized_range->range_is_sized);
    AssertEq(sized_range->lhs->value_expr->value, 0x1234);
    AssertEq(sized_range->rhs->value, 0x5678);

    Memmy_AstNode *target_endpoint_range = 0;
    Test_ParseAstExpr(arena, "[<1234!a.dll>..<1234!b.dll>]", &target_endpoint_range);
    AssertEq(target_endpoint_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(!target_endpoint_range->range_is_sized);
    AssertEq(target_endpoint_range->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(target_endpoint_range->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *process_range = 0;
    Test_ParseAstExpr(arena, "<game.exe!>", &process_range);
    AssertEq(process_range->kind, Memmy_AstNodeKind_Target);
    AssertTrue(process_range->target_has_process);
    AssertStrEq(process_range->target_process, String8_Lit("game.exe"));

    Memmy_AstNode *module_range = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module_range);
    AssertEq(module_range->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(module_range->target_module, String8_Lit("client.dll"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceAddresses)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *absolute = 0;
    Test_ParseAstExpr(arena, "@0x1234", &absolute);
    AssertEq(absolute->kind, Memmy_AstNodeKind_Address);
    AssertEq(absolute->value_expr->value, 0x1234);

    Memmy_AstNode *target_offset = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234", &target_offset);
    AssertEq(target_offset->kind, Memmy_AstNodeKind_Address);
    AssertEq(target_offset->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(target_offset->rhs->value, 0x1234);

    Memmy_AstNode *deref = 0;
    Test_ParseAstExpr(arena, "@0x1234->", &deref);
    AssertEq(deref->kind, Memmy_AstNodeKind_Deref);
    AssertTrue(deref->rhs == 0);
    AssertEq(deref->lhs->kind, Memmy_AstNodeKind_Address);

    Memmy_AstNode *deref_chain = 0;
    Test_ParseAstExpr(arena, "@0x1234->->", &deref_chain);
    AssertEq(deref_chain->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_chain->lhs->kind, Memmy_AstNodeKind_Deref);

    Memmy_AstNode *deref_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->0x42", &deref_offset);
    AssertEq(deref_offset->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_offset->rhs->value, 0x42);

    Memmy_AstNode *deref_negative_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->-0x42", &deref_negative_offset);
    AssertEq(deref_negative_offset->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_negative_offset->rhs->value, -0x42);

    Memmy_AstNode *deref_expr_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->(32 * (4 + 5))", &deref_expr_offset);
    AssertEq(deref_expr_offset->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_expr_offset->rhs->value, 32 * (4 + 5));

    Memmy_AstNode *range_deref = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]->", &range_deref);
    AssertEq(range_deref->kind, Memmy_AstNodeKind_Deref);
    AssertEq(range_deref->lhs->kind, Memmy_AstNodeKind_Range);

    Memmy_AstNode *target_deref = 0;
    Test_ParseAstExpr(arena, "<client.dll>->", &target_deref);
    AssertEq(target_deref->kind, Memmy_AstNodeKind_Deref);
    AssertEq(target_deref->lhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable_offset = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset", &variable_offset);
    AssertEq(variable_offset->kind, Memmy_AstNodeKind_Deref);
    AssertEq(variable_offset->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(variable_offset->rhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable_offset->rhs->name, String8_Lit("hp_offset"));

    Arena_Destroy(arena);
}

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

Test(Test_MemmyAstParsesPocketReferenceReads)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *absolute_read = 0;
    Test_ParseAstExpr(arena, "@0x1234 as u32", &absolute_read);
    AssertEq(absolute_read->kind, Memmy_AstNodeKind_TypedRead);
    AssertEq(absolute_read->lhs->kind, Memmy_AstNodeKind_Address);
    AssertStrEq(absolute_read->type_name, String8_Lit("u32"));

    Memmy_AstNode *deref_read = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234-> as str", &deref_read);
    AssertEq(deref_read->kind, Memmy_AstNodeKind_TypedRead);
    AssertEq(deref_read->lhs->kind, Memmy_AstNodeKind_Deref);
    AssertStrEq(deref_read->type_name, String8_Lit("str"));

    Memmy_AstNode *variable_read = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset as f32", &variable_read);
    AssertEq(variable_read->kind, Memmy_AstNodeKind_TypedRead);
    AssertEq(variable_read->lhs->kind, Memmy_AstNodeKind_Deref);
    AssertStrEq(variable_read->type_name, String8_Lit("f32"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceWrites)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *absolute_write = 0;
    Test_ParseAstExpr(arena, "@0x1234 as u32 = 0x42", &absolute_write);
    AssertEq(absolute_write->kind, Memmy_AstNodeKind_TypedWrite);
    AssertEq(absolute_write->lhs->kind, Memmy_AstNodeKind_Address);
    AssertStrEq(absolute_write->type_name, String8_Lit("u32"));
    AssertStrEq(absolute_write->value_text, String8_Lit("0x42"));

    Memmy_AstNode *string_write = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234-> as wstr = \"hello, world\"", &string_write);
    AssertEq(string_write->kind, Memmy_AstNodeKind_TypedWrite);
    AssertEq(string_write->lhs->kind, Memmy_AstNodeKind_Deref);
    AssertStrEq(string_write->type_name, String8_Lit("wstr"));
    AssertStrEq(string_write->value_text, String8_Lit("\"hello, world\""));

    Memmy_AstNode *float_write = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset as f32 = 100.0", &float_write);
    AssertEq(float_write->kind, Memmy_AstNodeKind_TypedWrite);
    AssertEq(float_write->lhs->kind, Memmy_AstNodeKind_Deref);
    AssertStrEq(float_write->type_name, String8_Lit("f32"));
    AssertStrEq(float_write->value_text, String8_Lit("100.0"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceAddressLists)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *module_pattern = 0;
    Test_ParseAstExpr(arena, "<client.dll>{AB CD ?? ?? 12 34}", &module_pattern);
    AssertEq(module_pattern->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(module_pattern->lhs->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(module_pattern->pattern, String8_Lit("AB CD ?? ?? 12 34"));

    Memmy_AstNode *process_pattern = 0;
    Test_ParseAstExpr(arena, "<game.exe!>{48 8B ? ? ? ? ? E8 ? ? ? ?}", &process_pattern);
    AssertEq(process_pattern->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(process_pattern->lhs->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(process_pattern->pattern, String8_Lit("48 8B ? ? ? ? ? E8 ? ? ? ?"));

    Memmy_AstNode *range_pattern = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]{ab cd ? ? 12 34}", &range_pattern);
    AssertEq(range_pattern->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(range_pattern->lhs->kind, Memmy_AstNodeKind_Range);
    AssertStrEq(range_pattern->pattern, String8_Lit("ab cd ? ? 12 34"));

    Memmy_AstNode *float_scan = 0;
    Test_ParseAstExpr(arena, "<client.dll> as f32 == 42.777", &float_scan);
    AssertEq(float_scan->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(float_scan->lhs->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(float_scan->type_name, String8_Lit("f32"));
    AssertStrEq(float_scan->value_text, String8_Lit("42.777"));

    Memmy_AstNode *string_scan = 0;
    Test_ParseAstExpr(arena, "<game.exe!> as str == \"hello\"", &string_scan);
    AssertEq(string_scan->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(string_scan->lhs->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(string_scan->type_name, String8_Lit("str"));
    AssertStrEq(string_scan->value_text, String8_Lit("\"hello\""));

    Memmy_AstNode *integer_scan = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678] as u32 == 123", &integer_scan);
    AssertEq(integer_scan->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(integer_scan->lhs->kind, Memmy_AstNodeKind_Range);
    AssertStrEq(integer_scan->type_name, String8_Lit("u32"));
    AssertStrEq(integer_scan->value_text, String8_Lit("123"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceIndexingAddressLists)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *pattern_index = 0;
    Test_ParseAstExpr(arena, "<client.dll>{ab cd ? ? 12 34}[0]", &pattern_index);
    AssertEq(pattern_index->kind, Memmy_AstNodeKind_Index);
    AssertEq(pattern_index->lhs->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(pattern_index->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(pattern_index->rhs->value, 0);

    Memmy_AstNode *value_scan_index = 0;
    Test_ParseAstExpr(arena, "(<client.dll> as f32 == 42.777)[2]", &value_scan_index);
    AssertEq(value_scan_index->kind, Memmy_AstNodeKind_Index);
    AssertEq(value_scan_index->lhs->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(value_scan_index->rhs->value, 2);

    Memmy_AstStatement matches_assignment = {0};
    Test_ParseAstStatement(arena, "$matches = <client.dll>{aa bb ?? ?? 11 22}", &matches_assignment);
    AssertEq(matches_assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(matches_assignment.assignment_value->kind, Memmy_AstNodeKind_PatternScan);
    AssertStrEq(matches_assignment.assignment_value->pattern, String8_Lit("aa bb ?? ?? 11 22"));

    Memmy_AstNode *variable_index = 0;
    Test_ParseAstExpr(arena, "$matches[3]", &variable_index);
    AssertEq(variable_index->kind, Memmy_AstNodeKind_Index);
    AssertEq(variable_index->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable_index->lhs->name, String8_Lit("matches"));
    AssertEq(variable_index->rhs->value, 3);

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

    Arena_Destroy(arena);
}

Test(Test_MemmyAstReportsPreciseDiagnosticOffsets)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("$foo + "), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(diagnostic.byte_offset, 7);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("0xg"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(diagnostic.byte_offset, 2);
    AssertEq(diagnostic.byte_count, 1);

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("1 2"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertEq(diagnostic.byte_offset, 2);
    AssertEq(diagnostic.byte_count, 1);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsOldAddressSpellings)
{
    Test_RejectAstExpr("0x1234->");
    Test_RejectAstExpr("address:+size");
    Test_RejectAstExpr("<1234!>0x1234");
}

Test(Test_MemmyAstParsesPocketReferenceCommands)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstStatement statement = {0};
    Test_ParseAstStatement(arena, "/procs game", &statement);
    AssertEq(statement.kind, Memmy_AstNodeKind_Command);
    AssertEq(statement.command_kind, Memmy_AstCommandKind_Procs);
    AssertStrEq(statement.command_arg, String8_Lit("game"));

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

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast = TestSuite_Make(
    "Memmy AST", TestCase_Make(Test_MemmyAstParsesConstantsWithPrecedence),
    TestCase_Make(Test_MemmyAstParsesVariablesInConstExpressions),
    TestCase_Make(Test_MemmyAstParsesParenthesizedTypedReadsInArithmetic),
    TestCase_Make(Test_MemmyAstRejectsInvalidIdentifiers), TestCase_Make(Test_MemmyAstParsesPocketReferenceTargets),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceCoreValues),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceRanges), TestCase_Make(Test_MemmyAstParsesPocketReferenceAddresses),
    TestCase_Make(Test_MemmyAstParsesAssignmentBasics), TestCase_Make(Test_MemmyAstReportsPreciseDiagnosticOffsets),
    TestCase_Make(Test_MemmyAstRejectsOldAddressSpellings), TestCase_Make(Test_MemmyAstParsesPocketReferenceReads),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceWrites),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceAddressLists),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceIndexingAddressLists),
    TestCase_Make(Test_MemmyAstParsesPocketReferencePhase4Assignments),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceCommands), );
