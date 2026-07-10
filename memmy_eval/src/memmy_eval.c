#include "memmy_eval_internal.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

void MemmyEval_Error_Set(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    Memmy_Error_Set(error, status, context, message);
}

Memmy_Status MemmyEval_Statement_Eval(Arena *out_arena, MemmyEval_Env *env, MemmyAst_Statement const *statement,
                                      MemmyEval_ResultSink const *sink, Memmy_Error *error)
{
    Scratch scratch = out_arena != 0 && env != 0 ? Scratch_Begin((Arena *[]){out_arena, env->arena}, 2) : (Scratch){0};
    MemmyEval_Exec exec = {.env = env, .out_arena = out_arena, .transient_arena = scratch.arena};
    Memmy_Status status = MemmyEval_Statement_EvalWithContext(&exec, statement, sink, error);
    MemmyEval_Exec_Close(&exec);
    if (scratch.arena != 0)
    {
        Scratch_End(scratch);
    }
    return status;
}

Memmy_Status MemmyEval_Expr_Eval(Arena *out_arena, MemmyEval_Env *env, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                 Memmy_Error *error)
{
    Scratch scratch = out_arena != 0 && env != 0 ? Scratch_Begin((Arena *[]){out_arena, env->arena}, 2) : (Scratch){0};
    MemmyEval_Exec exec = {.env = env, .out_arena = out_arena, .transient_arena = scratch.arena};
    Memmy_Status status = MemmyEval_Expr_EvalWithContext(&exec, expr, out, error);
    MemmyEval_Exec_Close(&exec);
    if (scratch.arena != 0)
    {
        Scratch_End(scratch);
    }
    return status;
}

Memmy_Status MemmyEval_Statement_EvalWithContext(MemmyEval_Exec *exec, MemmyAst_Statement const *statement,
                                                 MemmyEval_ResultSink const *sink, Memmy_Error *error)
{
    MemmyEval_Env *env = exec != 0 ? exec->env : 0;
    if (env == 0 || exec->out_arena == 0 || statement == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                            String8_Lit("missing eval environment or statement"));
        return Memmy_Status_InvalidArgument;
    }

    MemmyEval_Value value = {0};
    Memmy_Status status = Memmy_Status_Ok;
    if (statement->kind == MemmyAst_NodeKind_Assignment)
    {
        status = MemmyEval_Expr_EvalWithContext(exec, statement->assignment_value, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = MemmyEval_Env_Set(env, statement->assignment_name, value);
    }
    else if (statement->kind == MemmyAst_NodeKind_Command)
    {
        return MemmyEval_Command_Eval(exec, statement, sink, error);
    }
    else if (statement->expr != 0)
    {
        status = MemmyEval_Expr_EvalWithContext(exec, statement->expr, &value, error);
    }
    else
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("statement"),
                            String8_Lit("missing statement expression"));
        return Memmy_Status_InvalidArgument;
    }

    if (status == Memmy_Status_Ok)
    {
        status = MemmyEval_ValueResult_Emit(sink, value);
    }
    return status;
}

Memmy_Status MemmyEval_Expr_EvalWithContext(MemmyEval_Exec *exec, MemmyAst_Node const *expr, MemmyEval_Value *out,
                                            Memmy_Error *error)
{
    MemmyEval_Env *env = exec != 0 ? exec->env : 0;
    if (out != 0)
    {
        *out = (MemmyEval_Value){0};
    }
    if (env == 0 || exec->out_arena == 0 || expr == 0 || out == 0)
    {
        MemmyEval_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("eval"),
                            String8_Lit("missing eval environment, expression, or output"));
        return Memmy_Status_InvalidArgument;
    }

    switch (expr->kind)
    {
    case MemmyAst_NodeKind_ConstArithmetic:
    case MemmyAst_NodeKind_Variable:
    case MemmyAst_NodeKind_CurrentItem:
    case MemmyAst_NodeKind_ListTransform:
    case MemmyAst_NodeKind_Address:
    case MemmyAst_NodeKind_Range:
    case MemmyAst_NodeKind_Index:
        return MemmyEval_Expr_EvalValue(exec, expr, out, error);
    case MemmyAst_NodeKind_Target:
    case MemmyAst_NodeKind_ProcessRange:
    case MemmyAst_NodeKind_Function:
    case MemmyAst_NodeKind_ObjectBase:
        return MemmyEval_Expr_EvalProcess(exec, expr, out, error);
    case MemmyAst_NodeKind_Deref:
    case MemmyAst_NodeKind_TypedRead:
    case MemmyAst_NodeKind_TypedWrite:
        return MemmyEval_Expr_EvalMemory(exec, expr, out, error);
    case MemmyAst_NodeKind_PatternScan:
    case MemmyAst_NodeKind_ValueScan:
    case MemmyAst_NodeKind_ReferenceScan:
    case MemmyAst_NodeKind_DisasmScan:
        return MemmyEval_Expr_EvalScan(exec, expr, out, error);
    default:
        MemmyEval_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                            String8_Lit("expression kind is not implemented yet"));
        return Memmy_Status_Unsupported;
    }
}
