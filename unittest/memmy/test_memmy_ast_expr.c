#include "test_memmy_ast_common.h"

Test(Test_MemmyAstParsesConstantsWithPrecedence)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    Test_ParseAstExpr(arena, "0x1234 - 32 * (4 + 5)", &expr);

    AssertEq(expr->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(expr->op, MemmyAst_ConstOp_Sub);
    AssertEq(expr->lhs->value, 0x1234);
    AssertEq(expr->rhs->op, MemmyAst_ConstOp_Mul);
    AssertEq(expr->rhs->rhs->op, MemmyAst_ConstOp_Add);

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesVariablesInConstExpressions)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;
    Test_ParseAstExpr(arena, "$foo + 7 * $bar", &expr);

    AssertEq(expr->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertTrue(expr->contains_variable);
    AssertEq(expr->op, MemmyAst_ConstOp_Add);
    AssertEq(expr->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(expr->lhs->name, String8_Lit("foo"));
    AssertEq(expr->rhs->rhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(expr->rhs->rhs->name, String8_Lit("bar"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesGeneralAddressArithmetic)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *absolute_minus_module = 0;
    Test_ParseAstExpr(arena, "@0x10000123 - <client.dll>", &absolute_minus_module);
    AssertEq(absolute_minus_module->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(absolute_minus_module->op, MemmyAst_ConstOp_Sub);
    AssertEq(absolute_minus_module->lhs->kind, MemmyAst_NodeKind_Address);
    AssertEq(absolute_minus_module->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable_minus_module = 0;
    Test_ParseAstExpr(arena, "$hit - <client.dll>", &variable_minus_module);
    AssertEq(variable_minus_module->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(variable_minus_module->op, MemmyAst_ConstOp_Sub);
    AssertEq(variable_minus_module->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(variable_minus_module->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *module_plus_const = 0;
    Test_ParseAstExpr(arena, "<client.dll> + 0x123", &module_plus_const);
    AssertEq(module_plus_const->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(module_plus_const->op, MemmyAst_ConstOp_Add);
    AssertEq(module_plus_const->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(module_plus_const->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);

    MemmyAst_Node *const_plus_module = 0;
    Test_ParseAstExpr(arena, "0x123 + <client.dll>", &const_plus_module);
    AssertEq(const_plus_module->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(const_plus_module->op, MemmyAst_ConstOp_Add);
    AssertEq(const_plus_module->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(const_plus_module->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *parenthesized_deref = 0;
    Test_ParseAstExpr(arena, "($rva + <client.dll>)->($off + 0x42)->", &parenthesized_deref);
    AssertEq(parenthesized_deref->kind, MemmyAst_NodeKind_Deref);
    AssertEq(parenthesized_deref->lhs->kind, MemmyAst_NodeKind_Deref);
    AssertEq(parenthesized_deref->lhs->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(parenthesized_deref->lhs->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);

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
        MemmyAst_Node *expr = 0;
        MemmyAst_Diagnostic diagnostic = {0};
        AssertEq(MemmyAst_Expr_Parse(arena, rejected[i], &expr, &diagnostic), MemmyAst_Status_ParseError);
        AssertTrue(expr == 0);
        AssertStrEq(diagnostic.context, String8_Lit("ast"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyAstParsesPocketReferenceTargets)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *module = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module);
    AssertEq(module->kind, MemmyAst_NodeKind_Target);
    AssertStrEq(module->target_module, String8_Lit("client.dll"));

    MemmyAst_Node *process_range = 0;
    Test_ParseAstExpr(arena, "[0..]", &process_range);
    AssertEq(process_range->kind, MemmyAst_NodeKind_ProcessRange);

    Test_RejectAstExpr("<game.exe!>");
    Test_RejectAstExpr("<1234!>");
    Test_RejectAstExpr("<game.exe!client.dll>");
    Test_RejectAstExpr("<1234!client.dll>");

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceCoreValues)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *constant = 0;
    Test_ParseAstExpr(arena, "32 * (4 + 5)", &constant);
    AssertEq(constant->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(constant->op, MemmyAst_ConstOp_Mul);

    MemmyAst_Node *address = 0;
    Test_ParseAstExpr(arena, "@0x1234", &address);
    AssertEq(address->kind, MemmyAst_NodeKind_Address);
    AssertEq(address->value_expr->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(address->value_expr->value, 0x1234);

    MemmyAst_Node *explicit_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]", &explicit_range);
    AssertEq(explicit_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(!explicit_range->range_is_sized);
    AssertEq(explicit_range->lhs->kind, MemmyAst_NodeKind_Address);
    AssertEq(explicit_range->rhs->kind, MemmyAst_NodeKind_Address);

    MemmyAst_Node *sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..+0x5678]", &sized_range);
    AssertEq(sized_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(sized_range->range_is_sized);
    AssertEq(sized_range->lhs->kind, MemmyAst_NodeKind_Address);
    AssertEq(sized_range->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(sized_range->rhs->value, 0x5678);

    MemmyAst_Node *target = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &target);
    AssertEq(target->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable = 0;
    Test_ParseAstExpr(arena, "$name", &variable);
    AssertEq(variable->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable->name, String8_Lit("name"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceRanges)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *explicit_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]", &explicit_range);
    AssertEq(explicit_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(!explicit_range->range_is_sized);
    AssertEq(explicit_range->lhs->value_expr->value, 0x1234);
    AssertEq(explicit_range->rhs->value_expr->value, 0x5678);

    MemmyAst_Node *sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1234..+0x5678]", &sized_range);
    AssertEq(sized_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(sized_range->range_is_sized);
    AssertEq(sized_range->lhs->value_expr->value, 0x1234);
    AssertEq(sized_range->rhs->value, 0x5678);

    MemmyAst_Node *target_endpoint_range = 0;
    Test_ParseAstExpr(arena, "[<a.dll>..<b.dll>]", &target_endpoint_range);
    AssertEq(target_endpoint_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(!target_endpoint_range->range_is_sized);
    AssertEq(target_endpoint_range->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(target_endpoint_range->rhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *target_offset_sized_range = 0;
    Test_ParseAstExpr(arena, "[<a.dll>+0x10..+0x20]", &target_offset_sized_range);
    AssertEq(target_offset_sized_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(target_offset_sized_range->range_is_sized);
    AssertEq(target_offset_sized_range->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset_sized_range->lhs->op, MemmyAst_ConstOp_Add);
    AssertEq(target_offset_sized_range->rhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset_sized_range->rhs->value, 0x20);

    MemmyAst_Node *address_offset_sized_range = 0;
    Test_ParseAstExpr(arena, "[@0x1000 + 0x10..+0x20]", &address_offset_sized_range);
    AssertEq(address_offset_sized_range->kind, MemmyAst_NodeKind_Range);
    AssertTrue(address_offset_sized_range->range_is_sized);
    AssertEq(address_offset_sized_range->lhs->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(address_offset_sized_range->lhs->op, MemmyAst_ConstOp_Add);

    MemmyAst_Node *process_range = 0;
    Test_ParseAstExpr(arena, "[0..]", &process_range);
    AssertEq(process_range->kind, MemmyAst_NodeKind_ProcessRange);

    MemmyAst_Node *module_range = 0;
    Test_ParseAstExpr(arena, "<client.dll>", &module_range);
    AssertEq(module_range->kind, MemmyAst_NodeKind_Target);
    AssertStrEq(module_range->target_module, String8_Lit("client.dll"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesPocketReferenceAddresses)
{
    Arena *arena = Arena_CreateDefault();

    MemmyAst_Node *absolute = 0;
    Test_ParseAstExpr(arena, "@0x1234", &absolute);
    AssertEq(absolute->kind, MemmyAst_NodeKind_Address);
    AssertEq(absolute->value_expr->value, 0x1234);

    MemmyAst_Node *target_offset = 0;
    Test_ParseAstExpr(arena, "<client.dll>+0x1234", &target_offset);
    AssertEq(target_offset->kind, MemmyAst_NodeKind_ConstArithmetic);
    AssertEq(target_offset->op, MemmyAst_ConstOp_Add);
    AssertEq(target_offset->lhs->kind, MemmyAst_NodeKind_Target);
    AssertEq(target_offset->rhs->value, 0x1234);

    MemmyAst_Node *deref = 0;
    Test_ParseAstExpr(arena, "@0x1234->", &deref);
    AssertEq(deref->kind, MemmyAst_NodeKind_Deref);
    AssertTrue(deref->rhs == 0);
    AssertEq(deref->lhs->kind, MemmyAst_NodeKind_Address);

    MemmyAst_Node *deref_chain = 0;
    Test_ParseAstExpr(arena, "@0x1234->->", &deref_chain);
    AssertEq(deref_chain->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_chain->lhs->kind, MemmyAst_NodeKind_Deref);

    MemmyAst_Node *deref_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->0x42", &deref_offset);
    AssertEq(deref_offset->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_offset->rhs->value, 0x42);

    MemmyAst_Node *deref_negative_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->-0x42", &deref_negative_offset);
    AssertEq(deref_negative_offset->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_negative_offset->rhs->value, -0x42);

    MemmyAst_Node *deref_expr_offset = 0;
    Test_ParseAstExpr(arena, "@0x1234->(32 * (4 + 5))", &deref_expr_offset);
    AssertEq(deref_expr_offset->kind, MemmyAst_NodeKind_Deref);
    AssertEq(deref_expr_offset->rhs->op, MemmyAst_ConstOp_Mul);

    MemmyAst_Node *range_deref = 0;
    Test_ParseAstExpr(arena, "[@0x1234..@0x5678]->", &range_deref);
    AssertEq(range_deref->kind, MemmyAst_NodeKind_Deref);
    AssertEq(range_deref->lhs->kind, MemmyAst_NodeKind_Range);

    MemmyAst_Node *target_deref = 0;
    Test_ParseAstExpr(arena, "<client.dll>->", &target_deref);
    AssertEq(target_deref->kind, MemmyAst_NodeKind_Deref);
    AssertEq(target_deref->lhs->kind, MemmyAst_NodeKind_Target);

    MemmyAst_Node *variable_offset = 0;
    Test_ParseAstExpr(arena, "$player->$hp_offset", &variable_offset);
    AssertEq(variable_offset->kind, MemmyAst_NodeKind_Deref);
    AssertEq(variable_offset->lhs->kind, MemmyAst_NodeKind_Variable);
    AssertEq(variable_offset->rhs->kind, MemmyAst_NodeKind_Variable);
    AssertStrEq(variable_offset->rhs->name, String8_Lit("hp_offset"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstParsesOrdinaryFloatAndStringLiterals)
{
    Arena *arena = Arena_CreateDefault();
    MemmyAst_Node *expr = 0;

    Test_ParseAstExpr(arena, "42.777", &expr);
    AssertEq(expr->kind, MemmyAst_NodeKind_FloatLiteral);
    F64 expected = 42.777;
    U64 expected_bits = 0;
    Memory_Copy(&expected_bits, &expected, sizeof(expected_bits));
    AssertEq(expr->floating_bits, expected_bits);

    Test_ParseAstExpr(arena, "-0.0", &expr);
    AssertEq(expr->kind, MemmyAst_NodeKind_FloatLiteral);
    AssertEq(expr->floating_bits, 0x8000000000000000ull);

    Test_ParseAstExpr(arena, "\"hello\\n\\t\\\"\\\\world\"", &expr);
    AssertEq(expr->kind, MemmyAst_NodeKind_StringLiteral);
    AssertStrEq(expr->string, String8_Lit("hello\n\t\"\\world"));

    Test_RejectAstExpr("\"bad\\q\"");
    Test_RejectAstExpr("1e+");
    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_expr = TestSuite_Make(
    "Memmy AST Expressions", TestCase_Make(Test_MemmyAstParsesConstantsWithPrecedence),
    TestCase_Make(Test_MemmyAstParsesVariablesInConstExpressions),
    TestCase_Make(Test_MemmyAstParsesGeneralAddressArithmetic), TestCase_Make(Test_MemmyAstRejectsInvalidIdentifiers),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceTargets),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceCoreValues),
    TestCase_Make(Test_MemmyAstParsesPocketReferenceRanges), TestCase_Make(Test_MemmyAstParsesPocketReferenceAddresses),
    TestCase_Make(Test_MemmyAstParsesOrdinaryFloatAndStringLiterals), );
