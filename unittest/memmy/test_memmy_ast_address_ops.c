#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesFunctionLookup)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *function = 0;
    Test_ParseAstExpr(arena, "function $xref", &function);
    AssertEq(function->kind, Memmy_AstNodeKind_Function);
    AssertEq(function->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(function->lhs->name, String8_Lit("xref"));
    AssertTrue(function->contains_variable);

    Memmy_AstNode *transform = 0;
    Test_ParseAstExpr(arena, "$xrefs => function $", &transform);
    AssertEq(transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(transform->rhs->kind, Memmy_AstNodeKind_Function);
    AssertEq(transform->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);

    Memmy_AstNode *indexed_operand = 0;
    Test_ParseAstExpr(arena, "function $xrefs[0]", &indexed_operand);
    AssertEq(indexed_operand->kind, Memmy_AstNodeKind_Function);
    AssertEq(indexed_operand->lhs->kind, Memmy_AstNodeKind_Index);
    AssertEq(indexed_operand->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(indexed_operand->lhs->lhs->name, String8_Lit("xrefs"));

    Memmy_AstNode *typed_read = 0;
    Test_ParseAstExpr(arena, "function @0x1234 as u8", &typed_read);
    AssertEq(typed_read->kind, Memmy_AstNodeKind_TypedRead);
    AssertEq(typed_read->lhs->kind, Memmy_AstNodeKind_Function);

    Memmy_AstNode *target_offset = 0;
    Test_ParseAstExpr(arena, "function (<client.dll>+0x123)", &target_offset);
    AssertEq(target_offset->kind, Memmy_AstNodeKind_Function);
    AssertEq(target_offset->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset->lhs->op, Memmy_AstConstOp_Add);
    AssertEq(target_offset->lhs->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(target_offset->lhs->rhs->value, 0x123);

    Memmy_AstNode *ungrouped_target_offset = 0;
    Test_ParseAstExpr(arena, "function <client.dll>+0x123", &ungrouped_target_offset);
    AssertEq(ungrouped_target_offset->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(ungrouped_target_offset->op, Memmy_AstConstOp_Add);
    AssertEq(ungrouped_target_offset->lhs->kind, Memmy_AstNodeKind_Function);
    AssertEq(ungrouped_target_offset->lhs->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(ungrouped_target_offset->rhs->value, 0x123);

    Memmy_AstNode *lookup_result_rva = 0;
    Test_ParseAstExpr(arena, "function $xref - <client.dll>", &lookup_result_rva);
    AssertEq(lookup_result_rva->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(lookup_result_rva->op, Memmy_AstConstOp_Sub);
    AssertEq(lookup_result_rva->lhs->kind, Memmy_AstNodeKind_Function);
    AssertEq(lookup_result_rva->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(lookup_result_rva->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable_offset = 0;
    Test_ParseAstExpr(arena, "function ($xref + 4)", &variable_offset);
    AssertEq(variable_offset->kind, Memmy_AstNodeKind_Function);
    AssertEq(variable_offset->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(variable_offset->lhs->op, Memmy_AstConstOp_Add);
    AssertEq(variable_offset->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(variable_offset->lhs->rhs->value, 4);

    Memmy_AstNode *ungrouped_variable_offset = 0;
    Test_ParseAstExpr(arena, "function $xref + 4", &ungrouped_variable_offset);
    AssertEq(ungrouped_variable_offset->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(ungrouped_variable_offset->op, Memmy_AstConstOp_Add);
    AssertEq(ungrouped_variable_offset->lhs->kind, Memmy_AstNodeKind_Function);
    AssertEq(ungrouped_variable_offset->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(ungrouped_variable_offset->rhs->value, 4);

    Memmy_AstNode *target_offset_rva = 0;
    Test_ParseAstExpr(arena, "function (<client.dll>+0x123) - <client.dll>", &target_offset_rva);
    AssertEq(target_offset_rva->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset_rva->op, Memmy_AstConstOp_Sub);
    AssertEq(target_offset_rva->lhs->kind, Memmy_AstNodeKind_Function);
    AssertEq(target_offset_rva->lhs->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset_rva->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable = 0;
    Test_ParseAstExpr(arena, "$function", &variable);
    AssertEq(variable->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("function"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesObjectBaseLookup)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *absolute = 0;
    Test_ParseAstExpr(arena, "objectbase @0x1234", &absolute);
    AssertEq(absolute->kind, Memmy_AstNodeKind_ObjectBase);
    AssertEq(absolute->lhs->kind, Memmy_AstNodeKind_Address);

    Memmy_AstNode *variable_operand = 0;
    Test_ParseAstExpr(arena, "objectbase $addr", &variable_operand);
    AssertEq(variable_operand->kind, Memmy_AstNodeKind_ObjectBase);
    AssertEq(variable_operand->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable_operand->lhs->name, String8_Lit("addr"));
    AssertTrue(variable_operand->contains_variable);

    Memmy_AstNode *transform = 0;
    Test_ParseAstExpr(arena, "$hits => objectbase $", &transform);
    AssertEq(transform->kind, Memmy_AstNodeKind_ListTransform);
    AssertEq(transform->rhs->kind, Memmy_AstNodeKind_ObjectBase);
    AssertEq(transform->rhs->lhs->kind, Memmy_AstNodeKind_CurrentItem);

    Memmy_AstNode *typed_read = 0;
    Test_ParseAstExpr(arena, "objectbase @0x1234 as u8", &typed_read);
    AssertEq(typed_read->kind, Memmy_AstNodeKind_TypedRead);
    AssertEq(typed_read->lhs->kind, Memmy_AstNodeKind_ObjectBase);

    Memmy_AstNode *target_offset = 0;
    Test_ParseAstExpr(arena, "objectbase (<client.dll>+0x123)", &target_offset);
    AssertEq(target_offset->kind, Memmy_AstNodeKind_ObjectBase);
    AssertEq(target_offset->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset->lhs->op, Memmy_AstConstOp_Add);
    AssertEq(target_offset->lhs->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(target_offset->lhs->rhs->value, 0x123);

    Memmy_AstNode *lookup_result_rva = 0;
    Test_ParseAstExpr(arena, "objectbase $addr - <client.dll>", &lookup_result_rva);
    AssertEq(lookup_result_rva->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(lookup_result_rva->op, Memmy_AstConstOp_Sub);
    AssertEq(lookup_result_rva->lhs->kind, Memmy_AstNodeKind_ObjectBase);
    AssertEq(lookup_result_rva->lhs->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(lookup_result_rva->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable = 0;
    Test_ParseAstExpr(arena, "$objectbase", &variable);
    AssertEq(variable->kind, Memmy_AstNodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("objectbase"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsFunctionWithoutOperand)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("function"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    AssertStrEq(diagnostic.message, String8_Lit("expected address expression"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsObjectBaseWithoutOperand)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};

    AssertEq(Memmy_Ast_ParseExpr(arena, String8_Lit("objectbase"), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    AssertStrEq(diagnostic.message, String8_Lit("expected address expression"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_address_ops = TestSuite_Make(
    "Memmy AST Address Operations", TestCase_Make(Test_MemmyAstParsesFunctionLookup),
    TestCase_Make(Test_MemmyAstParsesObjectBaseLookup), TestCase_Make(Test_MemmyAstRejectsFunctionWithoutOperand),
    TestCase_Make(Test_MemmyAstRejectsObjectBaseWithoutOperand), );
