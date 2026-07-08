#include "memmy_ast.h"
#include "test_framework.h"

static void Test_ParseDisasmAstExpr(Arena *arena, char *text, Memmy_AstNode **out)
{
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), out, &diagnostic), Memmy_AstStatus_Ok);
    AssertTrue(*out != 0);
}

static void Test_RejectDisasmAstExpr(char *text)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Memmy_AstDiagnostic diagnostic = {0};
    AssertEq(Memmy_Ast_ParseExpr(arena, String8_FromCStr(text), &expr, &diagnostic), Memmy_AstStatus_ParseError);
    AssertTrue(expr == 0);
    AssertStrEq(diagnostic.context, String8_Lit("ast"));
    Arena_Destroy(arena);
}

Test(Test_MemmyAstDisasmX64ParsesScan)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseDisasmAstExpr(arena, "[@0x1000..+0x40] disasm x64 { mov reg, [rip+disp32]; xor rax, rax }", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_DisasmScan);
    AssertEq(expr->lhs->kind, Memmy_AstNodeKind_Range);
    AssertEq(expr->disasm_pattern.arch, Memmy_AstDisasmArch_X64);
    AssertEq(expr->disasm_pattern.instruction_count, 2);
    AssertEq(expr->disasm_pattern.instructions[0].operand_count, 2);
    AssertStrEq(expr->disasm_pattern.instructions[0].mnemonic, String8_Lit("mov"));
    AssertEq(expr->disasm_pattern.instructions[0].operands[0].kind, Memmy_AstDisasmOperandKind_RegisterAny);
    AssertEq(expr->disasm_pattern.instructions[0].operands[1].kind, Memmy_AstDisasmOperandKind_RipDisp32);
    AssertEq(expr->disasm_pattern.instructions[1].operand_count, 2);
    AssertStrEq(expr->disasm_pattern.instructions[1].mnemonic, String8_Lit("xor"));
    AssertEq(expr->disasm_pattern.instructions[1].operands[0].kind, Memmy_AstDisasmOperandKind_Register);
    AssertStrEq(expr->disasm_pattern.instructions[1].operands[0].reg, String8_Lit("rax"));

    Arena_Destroy(arena);
}

Test(Test_MemmyAstDisasmX64RejectsInvalidSyntax)
{
    Test_RejectDisasmAstExpr("[@0x1000..+0x40] disasm arm64 { mov reg, [rip+disp32] }");
    Test_RejectDisasmAstExpr("[@0x1000..+0x40] disasm x64 { }");
    Test_RejectDisasmAstExpr("[@0x1000..+0x40] disasm x64 { mov reg,, rax }");
    Test_RejectDisasmAstExpr("[@0x1000..+0x40] disasm x64 { mov reg, [rip+4] }");
}

Test(Test_MemmyAstDisasmX64LeavesNameResolutionToEval)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_AstNode *expr = 0;
    Test_ParseDisasmAstExpr(arena, "[@0x1000..+0x40] disasm x64 { nope maybe_reg }", &expr);

    AssertEq(expr->kind, Memmy_AstNodeKind_DisasmScan);
    AssertStrEq(expr->disasm_pattern.instructions[0].mnemonic, String8_Lit("nope"));
    AssertStrEq(expr->disasm_pattern.instructions[0].operands[0].reg, String8_Lit("maybe_reg"));

    Arena_Destroy(arena);
}

TestSuite suite_memmy_ast_disasm_x64 =
    TestSuite_Make("Memmy AST Disasm X64", TestCase_Make(Test_MemmyAstDisasmX64ParsesScan),
                   TestCase_Make(Test_MemmyAstDisasmX64RejectsInvalidSyntax),
                   TestCase_Make(Test_MemmyAstDisasmX64LeavesNameResolutionToEval), );
