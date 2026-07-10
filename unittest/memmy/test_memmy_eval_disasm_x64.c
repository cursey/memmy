#include "memmy_eval.h"
#include "test_memmy_backend.h"
#include "test_memmy_common.h"

static void Test_EvalDisasm_ParseExpr(Arena *arena, char *text, MemmyAst_Node **out)
{
    MemmyAst_Diagnostic diagnostic = {0};
    AssertEq(MemmyAst_Expr_Parse(arena, String8_FromCStr(text), out, &diagnostic), MemmyAst_Status_Ok);
    AssertTrue(*out != 0);
}

static void Test_EvalDisasm_ExprText(MemmyEval_Env *env, Arena *arena, char *text, MemmyEval_Value *out)
{
    MemmyAst_Node *expr = 0;
    Test_EvalDisasm_ParseExpr(arena, text, &expr);
    AssertEq(MemmyEval_Expr_Eval(arena, env, expr, out, 0), Memmy_Status_Ok);
}

static void Test_EvalDisasm_Setup(Arena *arena, Test_MemmyBackend *backend, MemmyEval_Env **out)
{
    Test_MemmyBackend_Init(backend);
    Test_MemmyBackend_AddProcess(backend, 4242, String8_Lit("game.exe"), String8_Lit("C:\\game\\game.exe"),
                                 Memmy_PointerWidth_64);
    Test_MemmyBackend_AddRegion(backend, 4242, 0x1000, TEST_MEMMY_BACKEND_MEMORY_SIZE,
                                Memmy_RegionAccess_Read | Memmy_RegionAccess_Write, Memmy_RegionState_Committed);

    Memmy_Context *ctx = Arena_PushStruct(arena, Memmy_Context);
    ctx->backend = Test_MemmyBackend_AsBackend(backend);
    Memmy_Context_Set(ctx);

    MemmyEval_Env *env = MemmyEval_Env_Create(arena);
    MemmyEval_Env_SetDefaultProcess(env, 4242, Memmy_PointerWidth_64);
    *out = env;
}

static void Test_EvalDisasm_WriteBytes(Test_MemmyBackend *backend, Memmy_Addr address, U8 *bytes, U64 count)
{
    U64 offset = address - backend->memory_base;
    for (U64 i = 0; i < count; i++)
    {
        backend->memory[offset + i] = bytes[i];
    }
}

Test(Test_MemmyEvalDisasmX64MatchesInstructionSequence)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalDisasm_Setup(arena, &backend, &env);

    U8 code[] = {0x8b, 0x05, 0x78, 0x56, 0x34, 0x12, 0x48, 0x31, 0xc0};
    Test_EvalDisasm_WriteBytes(&backend, 0x1010, code, ArrayCount(code));

    MemmyEval_Value matches = {0};
    Test_EvalDisasm_ExprText(env, arena, "[@0x1000..+0x40] disasm x64 { mov reg, [rip+disp32]; xor rax, rax }",
                             &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 1);
    AssertEq(matches.addresses[0], 0x1010);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalDisasmX64WildcardRegisterMatches)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalDisasm_Setup(arena, &backend, &env);

    U8 code[] = {0x8b, 0x0d, 0x78, 0x56, 0x34, 0x12};
    Test_EvalDisasm_WriteBytes(&backend, 0x1020, code, ArrayCount(code));

    MemmyEval_Value matches = {0};
    Test_EvalDisasm_ExprText(env, arena, "[@0x1000..+0x40] disasm x64 { mov reg, [rip+disp32] }", &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 1);
    AssertEq(matches.addresses[0], 0x1020);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalDisasmX64ExactRegisterMismatchFails)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalDisasm_Setup(arena, &backend, &env);

    U8 code[] = {0x8b, 0x0d, 0x78, 0x56, 0x34, 0x12};
    Test_EvalDisasm_WriteBytes(&backend, 0x1020, code, ArrayCount(code));

    MemmyEval_Value matches = {0};
    Test_EvalDisasm_ExprText(env, arena, "[@0x1000..+0x40] disasm x64 { mov rax, [rip+disp32] }", &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalDisasmX64NonConsecutiveInstructionFails)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalDisasm_Setup(arena, &backend, &env);

    U8 code[] = {0x8b, 0x05, 0x78, 0x56, 0x34, 0x12, 0x90, 0x48, 0x31, 0xc0};
    Test_EvalDisasm_WriteBytes(&backend, 0x1010, code, ArrayCount(code));

    MemmyEval_Value matches = {0};
    Test_EvalDisasm_ExprText(env, arena, "[@0x1000..+0x40] disasm x64 { mov reg, [rip+disp32]; xor rax, rax }",
                             &matches);
    AssertEq(matches.kind, MemmyEval_ValueKind_AddressList);
    AssertEq(matches.address_count, 0);

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

Test(Test_MemmyEvalDisasmX64RejectsUnknownMnemonicAndRegister)
{
    Arena *arena = Arena_CreateDefault();
    Test_MemmyBackend backend = {0};
    MemmyEval_Env *env = 0;
    Test_EvalDisasm_Setup(arena, &backend, &env);

    MemmyAst_Node *expr = 0;
    MemmyEval_Value value = {0};
    Memmy_Error error = {0};

    Test_EvalDisasm_ParseExpr(arena, "[@0x1000..+0x40] disasm x64 { nope reg }", &expr);
    AssertEq(MemmyEval_Expr_Eval(arena, env, expr, &value, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("disasm"));

    error = (Memmy_Error){0};
    Test_EvalDisasm_ParseExpr(arena, "[@0x1000..+0x40] disasm x64 { mov maybe_reg }", &expr);
    AssertEq(MemmyEval_Expr_Eval(arena, env, expr, &value, &error), Memmy_Status_ParseError);
    AssertStrEq(error.context, String8_Lit("disasm"));

    Memmy_Context_Set(0);
    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval_disasm_x64 =
    TestSuite_Make("Memmy Eval Disasm X64", TestCase_Make(Test_MemmyEvalDisasmX64MatchesInstructionSequence),
                   TestCase_Make(Test_MemmyEvalDisasmX64WildcardRegisterMatches),
                   TestCase_Make(Test_MemmyEvalDisasmX64ExactRegisterMismatchFails),
                   TestCase_Make(Test_MemmyEvalDisasmX64NonConsecutiveInstructionFails),
                   TestCase_Make(Test_MemmyEvalDisasmX64RejectsUnknownMnemonicAndRegister), );
