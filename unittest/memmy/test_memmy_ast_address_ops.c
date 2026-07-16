#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesNilLiteral)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *nil = 0;
    Test_ParseAstExpr(arena, "nil", &nil);
    AssertEq(nil->kind, MemmyAst_NodeKind_Nil);
    AssertStrEq(nil->text, String8_Lit("nil"));

    MemmyAst_Node *indexed = 0;
    Test_ParseAstExpr(arena, "nil[0]", &indexed);
    AssertEq(indexed->kind, MemmyAst_NodeKind_Index);
    AssertEq(indexed->lhs->kind, MemmyAst_NodeKind_Nil);

    MemmyAst_Node *transform = 0;
    Test_ParseAstExpr(arena, "nil => $ + 1", &transform);
    AssertEq(transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(transform->lhs->kind, MemmyAst_NodeKind_Nil);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesFunctionLookup)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *function = 0;
    Test_ParseAstExpr(arena, "function $xref", &function);
    AssertEq(function->kind, MemmyAst_NodeKind_Function);
    AssertEq(function->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(function->lhs->name, String8_Lit("xref"));
    AssertTrue(function->contains_variable);

    MemmyAst_Node *transform = 0;
    Test_ParseAstExpr(arena, "$xrefs => function $", &transform);
    AssertEq(transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(transform->rhs->kind, MemmyAst_NodeKind_Function);
    AssertEq(transform->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);

    MemmyAst_Node *indexed_operand = 0;
    Test_ParseAstExpr(arena, "function $xrefs[0]", &indexed_operand);
    AssertEq(indexed_operand->kind, MemmyAst_NodeKind_Function);
    AssertEq(indexed_operand->lhs->kind, MemmyAst_NodeKind_Index);
    AssertEq(indexed_operand->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(indexed_operand->lhs->lhs->name, String8_Lit("xrefs"));

    MemmyAst_Node *typed_read = 0;
    Test_ParseAstExpr(arena, "function @0x1234 as u8", &typed_read);
    AssertEq(typed_read->kind, MemmyAst_NodeKind_TypedRead);
    AssertEq(typed_read->lhs->kind, MemmyAst_NodeKind_Function);

    MemmyAst_Node *target_offset = 0;
    Test_ParseAstExpr(arena, "function (<client.dll>+0x123)", &target_offset);
    AssertEq(target_offset->kind, MemmyAst_NodeKind_Function);
    AssertEq(target_offset->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset->lhs->op, MemmyAst_ConstOp_Add);
    AssertEq(target_offset->lhs->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(target_offset->lhs->rhs->value, 0x123);

    MemmyAst_Node *ungrouped_target_offset = 0;
    Test_ParseAstExpr(arena, "function <client.dll>+0x123", &ungrouped_target_offset);
    AssertEq(ungrouped_target_offset->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(ungrouped_target_offset->op, MemmyAst_ConstOp_Add);
    AssertEq(ungrouped_target_offset->lhs->kind, MemmyAst_NodeKind_Function);
    AssertEq(ungrouped_target_offset->lhs->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(ungrouped_target_offset->rhs->value, 0x123);

    MemmyAst_Node *lookup_result_rva = 0;
    Test_ParseAstExpr(arena, "function $xref - <client.dll>", &lookup_result_rva);
    AssertEq(lookup_result_rva->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(lookup_result_rva->op, MemmyAst_ConstOp_Sub);
    AssertEq(lookup_result_rva->lhs->kind, MemmyAst_NodeKind_Function);
    AssertEq(lookup_result_rva->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(lookup_result_rva->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable_offset = 0;
    Test_ParseAstExpr(arena, "function ($xref + 4)", &variable_offset);
    AssertEq(variable_offset->kind, MemmyAst_NodeKind_Function);
    AssertEq(variable_offset->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(variable_offset->lhs->op, MemmyAst_ConstOp_Add);
    AssertEq(variable_offset->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(variable_offset->lhs->rhs->value, 4);

    MemmyAst_Node *ungrouped_variable_offset = 0;
    Test_ParseAstExpr(arena, "function $xref + 4", &ungrouped_variable_offset);
    AssertEq(ungrouped_variable_offset->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(ungrouped_variable_offset->op, MemmyAst_ConstOp_Add);
    AssertEq(ungrouped_variable_offset->lhs->kind, MemmyAst_NodeKind_Function);
    AssertEq(ungrouped_variable_offset->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(ungrouped_variable_offset->rhs->value, 4);

    MemmyAst_Node *target_offset_rva = 0;
    Test_ParseAstExpr(arena, "function (<client.dll>+0x123) - <client.dll>", &target_offset_rva);
    AssertEq(target_offset_rva->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset_rva->op, MemmyAst_ConstOp_Sub);
    AssertEq(target_offset_rva->lhs->kind, MemmyAst_NodeKind_Function);
    AssertEq(target_offset_rva->lhs->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset_rva->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable = 0;
    Test_ParseAstExpr(arena, "$function", &variable);
    AssertEq(variable->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("function"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesObjectBaseLookup)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *absolute = 0;
    Test_ParseAstExpr(arena, "objectbase @0x1234", &absolute);
    AssertEq(absolute->kind, MemmyAst_NodeKind_ObjectBase);
    AssertEq(absolute->lhs->kind, MemmyAst_NodeKind_Address);

    MemmyAst_Node *variable_operand = 0;
    Test_ParseAstExpr(arena, "objectbase $addr", &variable_operand);
    AssertEq(variable_operand->kind, MemmyAst_NodeKind_ObjectBase);
    AssertEq(variable_operand->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable_operand->lhs->name, String8_Lit("addr"));
    AssertTrue(variable_operand->contains_variable);

    MemmyAst_Node *transform = 0;
    Test_ParseAstExpr(arena, "$hits => objectbase $", &transform);
    AssertEq(transform->kind, MemmyAst_NodeKind_ListTransform);
    AssertEq(transform->rhs->kind, MemmyAst_NodeKind_ObjectBase);
    AssertEq(transform->rhs->lhs->kind, MemmyAst_NodeKind_CurrentItem);

    MemmyAst_Node *typed_read = 0;
    Test_ParseAstExpr(arena, "objectbase @0x1234 as u8", &typed_read);
    AssertEq(typed_read->kind, MemmyAst_NodeKind_TypedRead);
    AssertEq(typed_read->lhs->kind, MemmyAst_NodeKind_ObjectBase);

    MemmyAst_Node *target_offset = 0;
    Test_ParseAstExpr(arena, "objectbase (<client.dll>+0x123)", &target_offset);
    AssertEq(target_offset->kind, MemmyAst_NodeKind_ObjectBase);
    AssertEq(target_offset->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset->lhs->op, MemmyAst_ConstOp_Add);
    AssertEq(target_offset->lhs->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(target_offset->lhs->rhs->value, 0x123);

    MemmyAst_Node *lookup_result_rva = 0;
    Test_ParseAstExpr(arena, "objectbase $addr - <client.dll>", &lookup_result_rva);
    AssertEq(lookup_result_rva->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(lookup_result_rva->op, MemmyAst_ConstOp_Sub);
    AssertEq(lookup_result_rva->lhs->kind, MemmyAst_NodeKind_ObjectBase);
    AssertEq(lookup_result_rva->lhs->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(lookup_result_rva->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable = 0;
    Test_ParseAstExpr(arena, "$objectbase", &variable);
    AssertEq(variable->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("objectbase"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsFunctionWithoutOperand)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};

    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("function"), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    AssertStrEq(diagnostic.message, String8_Lit("expected address expression"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstRejectsObjectBaseWithoutOperand)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    MemmyAst_Diagnostic diagnostic = {0};

    AssertEq(MemmyAst_Expr_Parse(arena, String8_Lit("objectbase"), &expr, &diagnostic), MemmyAst_Status_ParseError);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    AssertStrEq(diagnostic.message, String8_Lit("expected address expression"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_address_ops =
    TestSuite_Make("Memmy AST Address Operations", TestCase_Make(Test_MemmyAstParsesNilLiteral),
                   TestCase_Make(Test_MemmyAstParsesFunctionLookup), TestCase_Make(Test_MemmyAstParsesObjectBaseLookup),
                   TestCase_Make(Test_MemmyAstRejectsFunctionWithoutOperand),
                   TestCase_Make(Test_MemmyAstRejectsObjectBaseWithoutOperand), );
