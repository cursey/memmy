#include "test_memmy_ast_common.h"

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

Test(Test_MemmyAstParsesGeneralAddressArithmetic)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_AstNode *absolute_minus_module = 0;
    Test_ParseAstExpr(arena, "@0x10000123 - <client.dll>", &absolute_minus_module);
    AssertEq(absolute_minus_module->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(absolute_minus_module->op, Memmy_AstConstOp_Sub);
    AssertEq(absolute_minus_module->lhs->kind, Memmy_AstNodeKind_Address);
    AssertEq(absolute_minus_module->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *variable_minus_module = 0;
    Test_ParseAstExpr(arena, "$hit - <client.dll>", &variable_minus_module);
    AssertEq(variable_minus_module->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(variable_minus_module->op, Memmy_AstConstOp_Sub);
    AssertEq(variable_minus_module->lhs->kind, Memmy_AstNodeKind_Variable);
    AssertEq(variable_minus_module->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *module_plus_const = 0;
    Test_ParseAstExpr(arena, "<client.dll> + 0x123", &module_plus_const);
    AssertEq(module_plus_const->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(module_plus_const->op, Memmy_AstConstOp_Add);
    AssertEq(module_plus_const->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(module_plus_const->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);

    Memmy_AstNode *const_plus_module = 0;
    Test_ParseAstExpr(arena, "0x123 + <client.dll>", &const_plus_module);
    AssertEq(const_plus_module->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(const_plus_module->op, Memmy_AstConstOp_Add);
    AssertEq(const_plus_module->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(const_plus_module->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *parenthesized_deref = 0;
    Test_ParseAstExpr(arena, "($rva + <client.dll>)->($off + 0x42)->", &parenthesized_deref);
    AssertEq(parenthesized_deref->kind, Memmy_AstNodeKind_Deref);
    AssertEq(parenthesized_deref->lhs->kind, Memmy_AstNodeKind_Deref);
    AssertEq(parenthesized_deref->lhs->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(parenthesized_deref->lhs->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);

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

    Memmy_AstNode *module = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module);
    AssertEq(module->kind, Memmy_AstNodeKind_Target);
    AssertStrEq(module->target_module, String8_Lit("client.dll"));

    Memmy_AstNode *process_range = 0;
    Test_ParseAstExpr(arena, "[0..]", &process_range);
    AssertEq(process_range->kind, Memmy_AstNodeKind_ProcessRange);

    Test_RejectAstExpr("<game.exe!>");
    Test_RejectAstExpr("<1234!>");
    Test_RejectAstExpr("<game.exe!client.dll>");
    Test_RejectAstExpr("<1234!client.dll>");

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
    Test_ParseAstExpr(arena, "[<a.dll>..<b.dll>]", &target_endpoint_range);
    AssertEq(target_endpoint_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(!target_endpoint_range->range_is_sized);
    AssertEq(target_endpoint_range->lhs->kind, Memmy_AstNodeKind_Target);
    AssertEq(target_endpoint_range->rhs->kind, Memmy_AstNodeKind_Target);

    Memmy_AstNode *target_offset_sized_range = 0;
    Test_ParseAstExpr(arena, "[<a.dll>+0x10..+0x20]", &target_offset_sized_range);
    AssertEq(target_offset_sized_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(target_offset_sized_range->range_is_sized);
    AssertEq(target_offset_sized_range->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset_sized_range->lhs->op, Memmy_AstConstOp_Add);
    AssertEq(target_offset_sized_range->rhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset_sized_range->rhs->value, 0x20);

    Memmy_AstNode *address_offset_sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1000 + 0x10..+0x20]", &address_offset_sized_range);
    AssertEq(address_offset_sized_range->kind, Memmy_AstNodeKind_Range);
    AssertTrue(address_offset_sized_range->range_is_sized);
    AssertEq(address_offset_sized_range->lhs->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(address_offset_sized_range->lhs->op, Memmy_AstConstOp_Add);

    Memmy_AstNode *process_range = 0;
    Test_ParseAstExpr(arena, "[0..]", &process_range);
    AssertEq(process_range->kind, Memmy_AstNodeKind_ProcessRange);

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
    AssertEq(target_offset->kind, Memmy_AstNodeKind_ConstArithmetic);
    AssertEq(target_offset->op, Memmy_AstConstOp_Add);
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

TestSuite suite_memmy_ast_expr = TestSuite_Make(
    "Memmy AST Expressions", TestCase_Make(Test_MemmyAstParsesConstantsWithPrecedence),
    TestCase_Make(Test_MemmyAstParsesVariablesInConstExpressions),
    TestCase_Make(Test_MemmyAstParsesGeneralAddressArithmetic), TestCase_Make(Test_MemmyAstRejectsInvalidIdentifiers),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceTargets),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceCoreValues),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceRanges),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceAddresses), );
