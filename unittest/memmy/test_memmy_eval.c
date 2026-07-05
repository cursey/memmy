#include "memmy_eval.h"
#include "test_framework.h"

Test(Test_MemmyEvalSmokeCreatesEnv)
{
    Arena *arena = Arena_CreateDefault();

    Memmy_EvalEnv *env = Memmy_EvalEnv_Create(arena);
    Memmy_EvalValue value = {.kind = Memmy_EvalValueKind_Const, .constant = 42};
    Memmy_EvalValue found = {0};

    AssertTrue(env != 0);
    AssertEq(Memmy_EvalExpr(env, 0, &found, 0), Memmy_Status_Unsupported);
    AssertEq(found.kind, Memmy_EvalValueKind_Null);
    AssertEq(Memmy_EvalEnv_Set(env, String8_Lit("x"), value), Memmy_Status_Unsupported);
    AssertEq(Memmy_EvalEnv_Find(env, String8_Lit("x"), &found), Memmy_Status_NotFound);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_eval = TestSuite_Make("Memmy Eval", TestCase_Make(Test_MemmyEvalSmokeCreatesEnv), );
