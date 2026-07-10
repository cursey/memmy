#include "test_memmy_ast_common.h"

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
    Test_ParseAstExpr(arena, "[0..]{48 8B ? ? ? ? ? E8 ? ? ? ?}", &process_pattern);
    AssertEq(process_pattern->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(process_pattern->lhs->kind, Memmy_AstNodeKind_ProcessRange);
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
    Test_ParseAstExpr(arena, "[0..] as str == \"hello\"", &string_scan);
    AssertEq(string_scan->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(string_scan->lhs->kind, Memmy_AstNodeKind_ProcessRange);
    AssertStrEq(string_scan->type_name, String8_Lit("str"));
    AssertStrEq(string_scan->value_text, String8_Lit("\"hello\""));

    Memmy_AstNode *integer_scan = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678] as u32 == 123", &integer_scan);
    AssertEq(integer_scan->kind, Memmy_AstNodeKind_ValueScan);
    AssertEq(integer_scan->lhs->kind, Memmy_AstNodeKind_Range);
    AssertStrEq(integer_scan->type_name, String8_Lit("u32"));
    AssertStrEq(integer_scan->value_text, String8_Lit("123"));

    Memmy_AstNode *module_ref = 0;
    Test_ParseAstExpr(arena, "<client.dll> refs ptr @0x1234", &module_ref);
    AssertEq(module_ref->kind, Memmy_AstNodeKind_ReferenceScan);
    AssertEq(module_ref->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(module_ref->rhs->kind, Memmy_AstNodeKind_Address);
    AssertEq(module_ref->reference_mode, Memmy_AstReferenceMode_Ptr);

    Memmy_AstNode *process_ref = 0;
    Test_ParseAstExpr(arena, "[0..] refs rel32 $target", &process_ref);
    AssertEq(process_ref->kind, Memmy_AstNodeKind_ReferenceScan);
    AssertEq(process_ref->lhs->kind, Memmy_AstNodeKind_ProcessRange);
    AssertEq(process_ref->rhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(process_ref->reference_mode, Memmy_AstReferenceMode_Rel32);

    Memmy_AstNode *range_ref = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678] refs any <client.dll>+0x20", &range_ref);
    AssertEq(range_ref->kind, Memmy_AstNodeKind_ReferenceScan);
    AssertEq(range_ref->lhs->kind, Memmy_AstNodeKind_Range);
    AssertEq(range_ref->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(range_ref->reference_mode, Memmy_AstReferenceMode_Any);

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

    Memmy_AstNode *indexed_arithmetic = 0;
    Test_ParseAstExpr(arena, "$xrefs[0] - 0xf", &indexed_arithmetic);
    AssertEq(indexed_arithmetic->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(indexed_arithmetic->op, Memmy_AstConstOp_Sub);
    AssertEq(indexed_arithmetic->lhs->kind, Memmy_AstNodeKind_Index);
    AssertEq(indexed_arithmetic->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(indexed_arithmetic->rhs->value, 0xf);

    Memmy_AstNode *scan_indexed_arithmetic = 0;
    Test_ParseAstExpr(arena, "(<client.dll>{aa})[0] - <client.dll>", &scan_indexed_arithmetic);
    AssertEq(scan_indexed_arithmetic->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(scan_indexed_arithmetic->op, Memmy_AstConstOp_Sub);
    AssertEq(scan_indexed_arithmetic->lhs->kind, Memmy_AstNodeKind_Index);
    AssertEq(scan_indexed_arithmetic->lhs->lhs->kind, Memmy_AstNodeKind_PatternScan);
    AssertEq(scan_indexed_arithmetic->rhs->kind, Memmy_AstNodeKind_Target);

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

    Memmy_AstNode *address_transform = 0;
    Test_ParseAstExpr(arena, "$refs => $ + 4", &address_transform);
    AssertEq(address_transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(address_transform->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(address_transform->lhs->name, String8_Lit("refs"));
    AssertEq(address_transform->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(address_transform->rhs->op, Memmy_AstConstOp_Add);
    AssertEq(address_transform->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);
    AssertEq(address_transform->rhs->rhs->value, 4);

    Memmy_AstNode *range_transform = 0;
    Test_ParseAstExpr(arena, "$refs => [$..+0x20]", &range_transform);
    AssertEq(range_transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(range_transform->rhs->kind, Memmy_AstNodeKind_Range);
    AssertTrue(range_transform->rhs->range_is_sized);
    AssertEq(range_transform->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);
    AssertEq(range_transform->rhs->rhs->value, 0x20);

    Memmy_AstNode *chained_transform = 0;
    Test_ParseAstExpr(arena, "$refs => [$..+0x20] => $ + 4", &chained_transform);
    AssertEq(chained_transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(chained_transform->lhs->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(chained_transform->lhs->rhs->kind, Memmy_AstNodeKind_Range);
    AssertEq(chained_transform->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(chained_transform->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);

    Memmy_AstNode *deref_precedence = 0;
    Test_ParseAstExpr(arena, "$refs => $->0x8", &deref_precedence);
    AssertEq(deref_precedence->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(deref_precedence->rhs->kind, Memmy_AstNodeKind_Deref);
    AssertEq(deref_precedence->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);

    Memmy_AstStatement assignment = {0};
    Test_ParseAstStatement(arena, "$ranges = $refs => [$..+0x20]", &assignment);
    AssertEq(assignment.kind, Memmy_AstNodeKind_Assignment);
    AssertEq(assignment.assignment_value->kind, Memmy_AstNodeKind_ListTransform);

    Memmy_AstNode *variable_range = 0;
    Test_ParseAstExpr(arena, "[$start..$end]", &variable_range);
    AssertEq(variable_range->kind, Memmy_AstNodeKind_Range);
    AssertEq(variable_range->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(variable_range->rhs->kind, Memmy_AstNodeKind_Variable);

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
