#include "memmy_dsl.h"
#include "test_framework.h"

static void Test_ParseStatement(Arena *arena, char *text, Memmy_Statement *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_Statement_Parse(arena, String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

static Memmy_AddressOp *Test_StatementAddressOpAt(Memmy_AddressExpr *expr, U64 index)
{
    U64 at = 0;
    List_ForEach(Memmy_AddressOp, op, &expr->ops, link)
    {
        if (at == index)
        {
            return op;
        }
        at++;
    }
    return 0;
}

Test(Test_MemmyDslStatementParsesCommands)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Statement statement = {0};

    Test_ParseStatement(arena, "/procs", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Procs);

    Test_ParseStatement(arena, "/vars", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Vars);

    Test_ParseStatement(arena, "/unset $target_1", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Unset);
    AssertStrEq(statement.variable.name, String8_Lit("target_1"));

    Test_ParseStatement(arena, "/help", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Help);

    Test_ParseStatement(arena, "/exit", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Exit);

    Test_ParseStatement(arena, "/quit", &statement);
    AssertEq(statement.kind, Memmy_StatementKind_Exit);

    Arena_Destroy(arena);
}

Test(Test_MemmyDslStatementRejectsBareCommands)
{
    String8 rejected[] = {
        String8_Lit("procs"), String8_Lit("vars"), String8_Lit("unset $target"),
        String8_Lit("help"),  String8_Lit("exit"), String8_Lit("quit"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_Statement statement = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Statement_Parse(arena, rejected[i], &statement, &error), Memmy_Status_ParseError);
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyDslStatementParsesMemoryExpressions)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Statement statement = {0};
    Test_ParseStatement(arena, "<client.dll>+0x123 : u32", &statement);

    AssertEq(statement.kind, Memmy_StatementKind_Memory);
    AssertEq(statement.memory.kind, Memmy_MemoryExprKind_Peek);
    AssertEq(statement.memory.address.base_kind, Memmy_AddressExprBaseKind_Target);

    Arena_Destroy(arena);
}

Test(Test_MemmyDslStatementParsesAssignmentPrecedence)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Statement address = {0};
    Memmy_Statement constant = {0};
    Memmy_Statement range = {0};

    Test_ParseStatement(arena, "$x = 100", &address);
    Test_ParseStatement(arena, "$x = (100)", &constant);
    Test_ParseStatement(arena, "$scan = <client.dll>[0x1000:+0x4000]", &range);

    AssertEq(address.kind, Memmy_StatementKind_Assignment);
    AssertStrEq(address.variable.name, String8_Lit("x"));
    AssertEq(address.assignment.kind, Memmy_VariableExprKind_Address);
    AssertEq(address.assignment.address.base_kind, Memmy_AddressExprBaseKind_Absolute);
    AssertEq(address.assignment.address.absolute, 100);

    AssertEq(constant.kind, Memmy_StatementKind_Assignment);
    AssertEq(constant.assignment.kind, Memmy_VariableExprKind_Const);
    AssertEq(constant.assignment.constant.kind, Memmy_ConstExprKind_Literal);
    AssertEq(constant.assignment.constant.value, 100);

    AssertEq(range.kind, Memmy_StatementKind_Assignment);
    AssertEq(range.assignment.kind, Memmy_VariableExprKind_Range);
    AssertEq(range.assignment.range.kind, Memmy_RangeExprKind_TargetSized);
    AssertEq(range.assignment.range.size, 0x4000);

    Arena_Destroy(arena);
}

Test(Test_MemmyDslStatementParsesVariableRefs)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_Statement address = {0};
    Memmy_Statement range = {0};
    Memmy_Statement constant = {0};

    Test_ParseStatement(arena, "$addr = $base+$offset->$next", &address);
    AssertEq(address.assignment.kind, Memmy_VariableExprKind_Address);
    AssertEq(address.assignment.address.base_kind, Memmy_AddressExprBaseKind_Variable);
    AssertStrEq(address.assignment.address.variable.name, String8_Lit("base"));
    AssertEq(address.assignment.address.ops.count, 2);

    Memmy_AddressOp *add = Test_StatementAddressOpAt(&address.assignment.address, 0);
    AssertTrue(add != 0);
    AssertEq(add->kind, Memmy_AddressOpKind_Add);
    AssertEq(add->offset_expr.kind, Memmy_ConstExprKind_Variable);
    AssertStrEq(add->offset_expr.variable.name, String8_Lit("offset"));

    Memmy_AddressOp *deref = Test_StatementAddressOpAt(&address.assignment.address, 1);
    AssertTrue(deref != 0);
    AssertEq(deref->kind, Memmy_AddressOpKind_DerefOffset);
    AssertEq(deref->offset_expr.kind, Memmy_ConstExprKind_Variable);
    AssertStrEq(deref->offset_expr.variable.name, String8_Lit("next"));

    Test_ParseStatement(arena, "$range = $base:+$size", &range);
    AssertEq(range.assignment.kind, Memmy_VariableExprKind_Range);
    AssertEq(range.assignment.range.kind, Memmy_RangeExprKind_AddressSized);
    AssertEq(range.assignment.range.address.base_kind, Memmy_AddressExprBaseKind_Variable);
    AssertStrEq(range.assignment.range.address.variable.name, String8_Lit("base"));
    AssertEq(range.assignment.range.size_expr.kind, Memmy_ConstExprKind_Variable);
    AssertStrEq(range.assignment.range.size_expr.variable.name, String8_Lit("size"));

    Test_ParseStatement(arena, "$count = ($lhs + 4)", &constant);
    AssertEq(constant.assignment.kind, Memmy_VariableExprKind_Const);
    AssertEq(constant.assignment.constant.kind, Memmy_ConstExprKind_Binary);
    AssertTrue(constant.assignment.constant.contains_variable);
    AssertEq(constant.assignment.constant.lhs->kind, Memmy_ConstExprKind_Variable);
    AssertStrEq(constant.assignment.constant.lhs->variable.name, String8_Lit("lhs"));

    Arena_Destroy(arena);
}

Test(Test_MemmyDslStatementRejectsInvalidVariableNames)
{
    String8 rejected[] = {
        String8_Lit("$ = 1"),
        String8_Lit("$1x = 1"),
        String8_Lit("$bad-name = 1"),
        String8_Lit("/unset $1x"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_Statement statement = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Statement_Parse(arena, rejected[i], &statement, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("statement"));
        Arena_Destroy(arena);
    }
}

Test(Test_MemmyDslStatementRejectsMalformedAssignments)
{
    String8 rejected[] = {
        String8_Lit("$x ="),
        String8_Lit("$x =="),
        String8_Lit("x = 1"),
        String8_Lit("$x = (1"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Arena *arena = Arena_CreateDefault();
        Memmy_Statement statement = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_Statement_Parse(arena, rejected[i], &statement, &error), Memmy_Status_ParseError);
        Arena_Destroy(arena);
    }
}

TestSuite suite_memmy_dsl_statement =
    TestSuite_Make("Memmy DSL Statement", TestCase_Make(Test_MemmyDslStatementParsesCommands),
                   TestCase_Make(Test_MemmyDslStatementRejectsBareCommands),
                   TestCase_Make(Test_MemmyDslStatementParsesMemoryExpressions),
                   TestCase_Make(Test_MemmyDslStatementParsesAssignmentPrecedence),
                   TestCase_Make(Test_MemmyDslStatementParsesVariableRefs),
                   TestCase_Make(Test_MemmyDslStatementRejectsInvalidVariableNames),
                   TestCase_Make(Test_MemmyDslStatementRejectsMalformedAssignments), );
