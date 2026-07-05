#include "memmy_dsl.h"
#include "test_framework.h"

static void Test_ParseTarget(char *text, Memmy_TargetExpr *out)
{
    Memmy_Error error = {0};
    AssertEq(Memmy_TargetExpr_Parse(String8_FromCStr(text), out, &error), Memmy_Status_Ok);
}

Test(Test_MemmyExprTargetParsesUnqualifiedModuleTarget)
{
    Memmy_TargetExpr target = {0};
    Test_ParseTarget("<client.dll>", &target);

    AssertEq(target.kind, Memmy_TargetExprKind_Module);
    AssertEq(target.process.kind, Memmy_ProcessSelectorKind_None);
    AssertStrEq(target.module_name, String8_Lit("client.dll"));
}

Test(Test_MemmyExprTargetParsesProcessNameQualifiedModuleTarget)
{
    Memmy_TargetExpr target = {0};
    Test_ParseTarget("<game.exe!client.dll>", &target);

    AssertEq(target.kind, Memmy_TargetExprKind_Module);
    AssertEq(target.process.kind, Memmy_ProcessSelectorKind_Name);
    AssertStrEq(target.process.name, String8_Lit("game.exe"));
    AssertStrEq(target.module_name, String8_Lit("client.dll"));
}

Test(Test_MemmyExprTargetParsesPidQualifiedModuleTarget)
{
    Memmy_TargetExpr target = {0};
    Test_ParseTarget("<123!client.dll>", &target);

    AssertEq(target.kind, Memmy_TargetExprKind_Module);
    AssertEq(target.process.kind, Memmy_ProcessSelectorKind_Pid);
    AssertEq(target.process.pid, 123);
    AssertStrEq(target.module_name, String8_Lit("client.dll"));
}

Test(Test_MemmyExprTargetParsesWholeProcessTargets)
{
    Memmy_TargetExpr name_target = {0};
    Memmy_TargetExpr pid_target = {0};
    Test_ParseTarget("<game.exe!>", &name_target);
    Test_ParseTarget("<123!>", &pid_target);

    AssertEq(name_target.kind, Memmy_TargetExprKind_WholeProcess);
    AssertEq(name_target.process.kind, Memmy_ProcessSelectorKind_Name);
    AssertStrEq(name_target.process.name, String8_Lit("game.exe"));
    AssertEq(name_target.module_name.len, 0);

    AssertEq(pid_target.kind, Memmy_TargetExprKind_WholeProcess);
    AssertEq(pid_target.process.kind, Memmy_ProcessSelectorKind_Pid);
    AssertEq(pid_target.process.pid, 123);
    AssertEq(pid_target.module_name.len, 0);
}

Test(Test_MemmyExprTargetRejectsEmptyTargetParts)
{
    String8 rejected[] = {
        String8_Lit("<>"),
        String8_Lit("<!client.dll>"),
        String8_Lit("<!>"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_TargetExpr target = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_TargetExpr_Parse(rejected[i], &target, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("target"));
    }
}

Test(Test_MemmyExprTargetRejectsInvalidNameCharacters)
{
    String8 rejected[] = {
        String8_Lit("<client<.dll>"),          String8_Lit("<client>.dll>"),
        String8_Lit("<game.exe!client!dll>"),  String8_Lit("<game<.exe!client.dll>"),
        String8_Lit("<game.exe!client>.dll>"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_TargetExpr target = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_TargetExpr_Parse(rejected[i], &target, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("target"));
    }
}

Test(Test_MemmyExprTargetRejectsBoundaryWhitespace)
{
    String8 rejected[] = {
        String8_Lit("< client.dll>"),          String8_Lit("<client.dll >"),
        String8_Lit("< game.exe!client.dll>"), String8_Lit("<game.exe !client.dll>"),
        String8_Lit("<game.exe! client.dll>"), String8_Lit("<game.exe!client.dll >"),
    };

    for (U64 i = 0; i < ArrayCount(rejected); i++)
    {
        Memmy_TargetExpr target = {0};
        Memmy_Error error = {0};
        AssertEq(Memmy_TargetExpr_Parse(rejected[i], &target, &error), Memmy_Status_ParseError);
        AssertStrEq(error.context, String8_Lit("target"));
    }
}

Test(Test_MemmyExprTargetPreservesNumericModuleName)
{
    Memmy_TargetExpr target = {0};
    Test_ParseTarget("<123>", &target);

    AssertEq(target.kind, Memmy_TargetExprKind_Module);
    AssertEq(target.process.kind, Memmy_ProcessSelectorKind_None);
    AssertStrEq(target.module_name, String8_Lit("123"));
}

TestSuite suite_memmy_dsl_target =
    TestSuite_Make("Memmy DSL Target", TestCase_Make(Test_MemmyExprTargetParsesUnqualifiedModuleTarget),
                   TestCase_Make(Test_MemmyExprTargetParsesProcessNameQualifiedModuleTarget),
                   TestCase_Make(Test_MemmyExprTargetParsesPidQualifiedModuleTarget),
                   TestCase_Make(Test_MemmyExprTargetParsesWholeProcessTargets),
                   TestCase_Make(Test_MemmyExprTargetRejectsEmptyTargetParts),
                   TestCase_Make(Test_MemmyExprTargetRejectsInvalidNameCharacters),
                   TestCase_Make(Test_MemmyExprTargetRejectsBoundaryWhitespace),
                   TestCase_Make(Test_MemmyExprTargetPreservesNumericModuleName), );
