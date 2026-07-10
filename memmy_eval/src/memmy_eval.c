#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

void Memmy_EvalError(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    Memmy_Error_Set(error, status, context, message);
}

Memmy_Status Memmy_EvalStatement(Memmy_EvalEnv *env, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                 Memmy_Error *error)
{
    Scratch scratch = env != 0 ? Scratch_Begin(&env->arena, 1) : (Scratch){0};
    Memmy_EvalExec exec = {.env = env, .transient_arena = scratch.arena};
    Memmy_Status status = Memmy_EvalStatementWithContext(&exec, statement, sink, error);
    Memmy_EvalExec_Close(&exec);
    if (scratch.arena != 0)
    {
        Scratch_End(scratch);
    }
    return status;
}

Memmy_Status Memmy_EvalExpr(Memmy_EvalEnv *env, Memmy_AstNode *expr, Memmy_EvalValue *out, Memmy_Error *error)
{
    Scratch scratch = env != 0 ? Scratch_Begin(&env->arena, 1) : (Scratch){0};
    Memmy_EvalExec exec = {.env = env, .transient_arena = scratch.arena};
    Memmy_Status status = Memmy_EvalExprWithContext(&exec, expr, out, error);
    Memmy_EvalExec_Close(&exec);
    if (scratch.arena != 0)
    {
        Scratch_End(scratch);
    }
    return status;
}

Memmy_Status Memmy_EvalStatementWithContext(Memmy_EvalExec *exec, Memmy_AstStatement *statement,
                                            Memmy_EvalResultSink *sink, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec != 0 ? exec->env : 0;
    if (env == 0 || statement == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                        String8_Lit("missing eval environment or statement"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalValue value = {0};
    Memmy_Status status = Memmy_Status_Ok;
    if (statement->kind == Memmy_AstNodeKind_Assignment)
    {
        status = Memmy_EvalExprWithContext(exec, statement->assignment_value, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_EvalEnv_Set(env, statement->assignment_name, value);
    }
    else if (statement->kind == Memmy_AstNodeKind_Command)
    {
        return Memmy_Eval_Command(exec, statement, sink, error);
    }
    else if (statement->expr != 0)
    {
        status = Memmy_EvalExprWithContext(exec, statement->expr, &value, error);
    }
    else
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                        String8_Lit("missing statement expression"));
        return Memmy_Status_InvalidArgument;
    }

    if (status == Memmy_Status_Ok)
    {
        Memmy_Eval_EmitValueResult(sink, value);
    }
    return status;
}

Memmy_Status Memmy_EvalExprWithContext(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out,
                                       Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec != 0 ? exec->env : 0;
    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }
    if (env == 0 || expr == 0 || out == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                        String8_Lit("missing eval environment, expression, or output"));
        return Memmy_Status_InvalidArgument;
    }

    switch (expr->kind)
    {
    case Memmy_AstNodeKind_ConstArithmetic:
    case Memmy_AstNodeKind_Variable:
    case Memmy_AstNodeKind_CurrentItem:
    case Memmy_AstNodeKind_ListTransform:
    case Memmy_AstNodeKind_Address:
    case Memmy_AstNodeKind_Range:
    case Memmy_AstNodeKind_Index:
        return Memmy_Eval_ValueExpr(exec, expr, out, error);
    case Memmy_AstNodeKind_Target:
    case Memmy_AstNodeKind_ProcessRange:
    case Memmy_AstNodeKind_Function:
    case Memmy_AstNodeKind_ObjectBase:
        return Memmy_Eval_ProcessExpr(exec, expr, out, error);
    case Memmy_AstNodeKind_Deref:
    case Memmy_AstNodeKind_TypedRead:
    case Memmy_AstNodeKind_TypedWrite:
        return Memmy_Eval_MemoryExpr(exec, expr, out, error);
    case Memmy_AstNodeKind_PatternScan:
    case Memmy_AstNodeKind_ValueScan:
    case Memmy_AstNodeKind_ReferenceScan:
    case Memmy_AstNodeKind_DisasmScan:
        return Memmy_Eval_ScanExpr(exec, expr, out, error);
    default:
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                        String8_Lit("expression kind is not implemented yet"));
        return Memmy_Status_Unsupported;
    }
}
