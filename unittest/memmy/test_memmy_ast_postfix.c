#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesParenthesizedTypedReadsInArithmetic)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    Test_ParseAstExpr(arena, "$entity_list_mov + 7 + ($entity_list_mov + 3 as i32)", &expr);

    AssertEq(expr->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(expr->op, MemmyAst_ConstOp_Add);
    AssertEq(expr->rhs->kind, MemmyAst_NodeKind_TypedRead);
    AssertStrEq(expr->rhs->type_name, String8_Lit("i32"));
    AssertEq(expr->rhs->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(expr->rhs->lhs->op, MemmyAst_ConstOp_Add);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceReads)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *absolute_read = 0;
    Test_ParseAstExpr(arena, "@0x1234 as u32", &absolute_read);
    AssertEq(absolute_read->kind, MemmyAst_NodeKind_TypedRead);
    AssertEq(absolute_read->lhs->kind, MemmyAst_NodeKind_Address);
    AssertStrEq(absolute_read->type_name, String8_Lit("u32"));

    MemmyAst_Node *deref_read = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234-> as str", &deref_read);
    AssertEq(deref_read->kind, MemmyAst_NodeKind_TypedRead);
    AssertEq(deref_read->lhs->kind, MemmyAst_NodeKind_Deref);
    AssertStrEq(deref_read->type_name, String8_Lit("str"));

    MemmyAst_Node *variable_read = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset as f32", &variable_read);
    AssertEq(variable_read->kind, MemmyAst_NodeKind_TypedRead);
    AssertEq(variable_read->lhs->kind, MemmyAst_NodeKind_Deref);
    AssertStrEq(variable_read->type_name, String8_Lit("f32"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceWrites)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *absolute_write = 0;
    Test_ParseAstExpr(arena, "@0x1234 as u32 = 0x42", &absolute_write);
    AssertEq(absolute_write->kind, MemmyAst_NodeKind_TypedWrite);
    AssertEq(absolute_write->lhs->kind, MemmyAst_NodeKind_Address);
    AssertStrEq(absolute_write->type_name, String8_Lit("u32"));
    AssertStrEq(absolute_write->value_text, String8_Lit("0x42"));

    MemmyAst_Node *string_write = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234-> as wstr = \"hello, world\"", &string_write);
    AssertEq(string_write->kind, MemmyAst_NodeKind_TypedWrite);
    AssertEq(string_write->lhs->kind, MemmyAst_NodeKind_Deref);
    AssertStrEq(string_write->type_name, String8_Lit("wstr"));
    AssertStrEq(string_write->value_text, String8_Lit("\"hello, world\""));

    MemmyAst_Node *float_write = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset as f32 = 100.0", &float_write);
    AssertEq(float_write->kind, MemmyAst_NodeKind_TypedWrite);
    AssertEq(float_write->lhs->kind, MemmyAst_NodeKind_Deref);
    AssertStrEq(float_write->type_name, String8_Lit("f32"));
    AssertStrEq(float_write->value_text, String8_Lit("100.0"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceAddressLists)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *module_pattern = 0;
    Test_ParseAstExpr(arena, "<client.dll>{AB CD ?? ?? 12 34}", &module_pattern);
    AssertEq(module_pattern->kind, MemmyAst_NodeKind_PatternScan);
    AssertEq(module_pattern->lhs->kind, MemmyAst_NodeKind_Target);
    AssertStrEq(module_pattern->pattern, String8_Lit("AB CD ?? ?? 12 34"));

    MemmyAst_Node *process_pattern = 0;
    Test_ParseAstExpr(arena, "[0..]{48 8B ? ? ? ? ? E8 ? ? ? ?}", &process_pattern);
    AssertEq(process_pattern->kind, MemmyAst_NodeKind_PatternScan);
    AssertEq(process_pattern->lhs->kind, MemmyAst_NodeKind_ProcessRange);
    AssertStrEq(process_pattern->pattern, String8_Lit("48 8B ? ? ? ? ? E8 ? ? ? ?"));

    MemmyAst_Node *range_pattern = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]{ab cd ? ? 12 34}", &range_pattern);
    AssertEq(range_pattern->kind, MemmyAst_NodeKind_PatternScan);
    AssertEq(range_pattern->lhs->kind, MemmyAst_NodeKind_Range);
    AssertStrEq(range_pattern->pattern, String8_Lit("ab cd ? ? 12 34"));

    MemmyAst_Node *float_scan = 0;
    Test_ParseAstExpr(arena, "<client.dll> as f32 == 42.777", &float_scan);
    AssertEq(float_scan->kind, MemmyAst_NodeKind_ValueScan);
    AssertEq(float_scan->lhs->kind, MemmyAst_NodeKind_Target);
    AssertStrEq(float_scan->type_name, String8_Lit("f32"));
    AssertStrEq(float_scan->value_text, String8_Lit("42.777"));

    MemmyAst_Node *string_scan = 0;
    Test_ParseAstExpr(arena, "[0..] as str == \"hello\"", &string_scan);
    AssertEq(string_scan->kind, MemmyAst_NodeKind_ValueScan);
    AssertEq(string_scan->lhs->kind, MemmyAst_NodeKind_ProcessRange);
    AssertStrEq(string_scan->type_name, String8_Lit("str"));
    AssertStrEq(string_scan->value_text, String8_Lit("\"hello\""));

    MemmyAst_Node *integer_scan = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678] as u32 == 123", &integer_scan);
    AssertEq(integer_scan->kind, MemmyAst_NodeKind_ValueScan);
    AssertEq(integer_scan->lhs->kind, MemmyAst_NodeKind_Range);
    AssertStrEq(integer_scan->type_name, String8_Lit("u32"));
    AssertStrEq(integer_scan->value_text, String8_Lit("123"));

    MemmyAst_Node *module_ref = 0;
    Test_ParseAstExpr(arena, "<client.dll> refs ptr @0x1234", &module_ref);
    AssertEq(module_ref->kind, MemmyAst_NodeKind_ReferenceScan);
    AssertEq(module_ref->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(module_ref->rhs->kind, MemmyAst_NodeKind_Address);
    AssertEq(module_ref->reference_mode, MemmyAst_ReferenceMode_Ptr);

    MemmyAst_Node *process_ref = 0;
    Test_ParseAstExpr(arena, "[0..] refs rel32 $target", &process_ref);
    AssertEq(process_ref->kind, MemmyAst_NodeKind_ReferenceScan);
    AssertEq(process_ref->lhs->kind, MemmyAst_NodeKind_ProcessRange);
    AssertEq(process_ref->rhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(process_ref->reference_mode, MemmyAst_ReferenceMode_Rel32);

    MemmyAst_Node *range_ref = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678] refs any <client.dll>+0x20", &range_ref);
    AssertEq(range_ref->kind, MemmyAst_NodeKind_ReferenceScan);
    AssertEq(range_ref->lhs->kind, MemmyAst_NodeKind_Range);
    AssertEq(range_ref->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(range_ref->reference_mode, MemmyAst_ReferenceMode_Any);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceIndexingAddressLists)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *pattern_index = 0;
    Test_ParseAstExpr(arena, "<client.dll>{ab cd ? ? 12 34}[0]", &pattern_index);
    AssertEq(pattern_index->kind, MemmyAst_NodeKind_Index);
    AssertEq(pattern_index->lhs->kind, MemmyAst_NodeKind_PatternScan);
    AssertEq(pattern_index->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(pattern_index->rhs->value, 0);

    MemmyAst_Node *value_scan_index = 0;
    Test_ParseAstExpr(arena, "(<client.dll> as f32 == 42.777)[2]", &value_scan_index);
    AssertEq(value_scan_index->kind, MemmyAst_NodeKind_Index);
    AssertEq(value_scan_index->lhs->kind, MemmyAst_NodeKind_ValueScan);
    AssertEq(value_scan_index->rhs->value, 2);

    MemmyAst_Statement matches_assignment = {0};
    Test_ParseAstStatement(arena, "$matches = <client.dll>{aa bb ?? ?? 11 22}", &matches_assignment);
    AssertEq(matches_assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(matches_assignment.assignment_value->kind, MemmyAst_NodeKind_PatternScan);
    AssertStrEq(matches_assignment.assignment_value->pattern, String8_Lit("aa bb ?? ?? 11 22"));

    MemmyAst_Node *variable_index = 0;
    Test_ParseAstExpr(arena, "$matches[3]", &variable_index);
    AssertEq(variable_index->kind, MemmyAst_NodeKind_Index);
    AssertEq(variable_index->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable_index->lhs->name, String8_Lit("matches"));
    AssertEq(variable_index->rhs->value, 3);

    MemmyAst_Node *indexed_arithmetic = 0;
    Test_ParseAstExpr(arena, "$xrefs[0] - 0xf", &indexed_arithmetic);
    AssertEq(indexed_arithmetic->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(indexed_arithmetic->op, MemmyAst_ConstOp_Sub);
    AssertEq(indexed_arithmetic->lhs->kind, MemmyAst_NodeKind_Index);
    AssertEq(indexed_arithmetic->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(indexed_arithmetic->rhs->value, 0xf);

    MemmyAst_Node *scan_indexed_arithmetic = 0;
    Test_ParseAstExpr(arena, "(<client.dll>{aa})[0] - <client.dll>", &scan_indexed_arithmetic);
    AssertEq(scan_indexed_arithmetic->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(scan_indexed_arithmetic->op, MemmyAst_ConstOp_Sub);
    AssertEq(scan_indexed_arithmetic->lhs->kind, MemmyAst_NodeKind_Index);
    AssertEq(scan_indexed_arithmetic->lhs->lhs->kind, MemmyAst_NodeKind_PatternScan);
    AssertEq(scan_indexed_arithmetic->rhs->kind, MemmyAst_NodeKind_Target);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsInvalidReferenceScans)
{
    Test_RejectAstExpr("[0..] refs");
    Test_RejectAstExpr("[0..] refs wat @0x1000");
    Test_RejectAstExpr("[0..] refs ptr");
    Test_RejectAstExpr("[0..] refs ptr 1234");
}

Test(Test_MemmyAstParsesListTransforms)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *address_transform = 0;
    Test_ParseAstExpr(arena, "$refs => $ + 4", &address_transform);
    AssertEq(address_transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(address_transform->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(address_transform->lhs->name, String8_Lit("refs"));
    AssertEq(address_transform->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(address_transform->rhs->op, MemmyAst_ConstOp_Add);
    AssertEq(address_transform->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);
    AssertEq(address_transform->rhs->rhs->value, 4);

    MemmyAst_Node *range_transform = 0;
    Test_ParseAstExpr(arena, "$refs => [$..+0x20]", &range_transform);
    AssertEq(range_transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(range_transform->rhs->kind, MemmyAst_NodeKind_Range);
    AssertTrue(range_transform->rhs->range_is_sized);
    AssertEq(range_transform->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);
    AssertEq(range_transform->rhs->rhs->value, 0x20);

    MemmyAst_Node *chained_transform = 0;
    Test_ParseAstExpr(arena, "$refs => [$..+0x20] => $ + 4", &chained_transform);
    AssertEq(chained_transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(chained_transform->lhs->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(chained_transform->lhs->rhs->kind, MemmyAst_NodeKind_Range);
    AssertEq(chained_transform->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(chained_transform->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);

    MemmyAst_Node *deref_precedence = 0;
    Test_ParseAstExpr(arena, "$refs => $->0x8", &deref_precedence);
    AssertEq(deref_precedence->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(deref_precedence->rhs->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_precedence->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);

    MemmyAst_Statement assignment = {0};
    Test_ParseAstStatement(arena, "$ranges = $refs => [$..+0x20]", &assignment);
    AssertEq(assignment.kind, MemmyAst_NodeKind_Assignment);
    AssertEq(assignment.assignment_value->kind, MemmyAst_NodeKind_ListTransform);

    MemmyAst_Node *variable_range = 0;
    Test_ParseAstExpr(arena, "[$start..$end]", &variable_range);
    AssertEq(variable_range->kind, MemmyAst_NodeKind_Range);
    AssertEq(variable_range->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(variable_range->rhs->kind, MemmyAst_NodeKind_Variable);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsInvalidListTransforms)
{
    Test_RejectAstExpr("$");
    Test_RejectAstExpr("$refs =>");
    Test_RejectAstExpr("$refs => $bad =>");
    Test_RejectAstExpr("[$..+0x20]");
}

TestSuite suite_memmy_ast_postfix = TestSuite_Make(
    "Memmy AST Postfix", TestCase_Make(Test_MemmyAstParsesParenthesizedTypedReadsInArithmetic),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceReads), TestCase_Make(Test_MemmyAstParsesPocketReferenceWrites),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceAddressLists),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceIndexingAddressLists),
    TestCase_Make(Test_MemmyAstRejectsInvalidReferenceScans), TestCase_Make(Test_MemmyAstParsesListTransforms),
    TestCase_Make(Test_MemmyAstRejectsInvalidListTransforms), );
