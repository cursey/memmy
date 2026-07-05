#include "memmy_exec.h"

#include "base_checked.h"

static Memmy_Status Memmy_TargetExpr_CheckProcess(Memmy_Process *process, Memmy_TargetExpr *target, Memmy_Error *error)
{
    if (target->process.kind == Memmy_ProcessSelectorKind_None)
    {
        return Memmy_Status_Ok;
    }

    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing selected process for process-qualified target"));
        return Memmy_Status_InvalidArgument;
    }

    if (target->process.kind == Memmy_ProcessSelectorKind_Pid)
    {
        if (process->pid != target->process.pid)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                            String8_Lit("selected process does not match expression pid selector"));
            return Memmy_Status_InvalidArgument;
        }
    }
    return Memmy_Status_Ok;
}

typedef struct Memmy_TargetExprModuleResolver Memmy_TargetExprModuleResolver;
struct Memmy_TargetExprModuleResolver
{
    Memmy_TargetExpr *target;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};

static Memmy_Status Memmy_TargetExprModuleResolver_Push(void *user_data, Memmy_Module *module)
{
    Memmy_TargetExprModuleResolver *resolver = (Memmy_TargetExprModuleResolver *)user_data;
    if (!String8_EqNoCase(module->name, resolver->target->module_name))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        Memmy_Error_Set(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                        String8_Lit("module target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *module;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_TargetExpr_ResolveModuleBase(Memmy_Process *process, Memmy_TargetExpr *target,
                                                       Memmy_Addr *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_TargetExpr_CheckProcess(process, target, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(0, 0);
    Memmy_TargetExprModuleResolver resolver = {
        .target = target,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_TargetExprModuleResolver_Push,
        .user_data = &resolver,
    };
    status = Memmy_Process_EnumerateModules(scratch.arena, process, sink, error);
    Scratch_End(scratch);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (resolver.match_count == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = resolver.match.base;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressExpr_ApplyOffset(Memmy_Addr addr, I64 offset, Memmy_Addr *out, Memmy_Error *error)
{
    if (!AddI64ToU64Checked(addr, offset, out))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressExpr_ApplySubOffset(Memmy_Addr addr, I64 offset, Memmy_Addr *out, Memmy_Error *error)
{
    B32 ok = 0;
    if (offset >= 0)
    {
        ok = SubU64Checked(addr, (U64)offset, out);
    }
    else
    {
        U64 magnitude = (U64)(-(offset + 1)) + 1;
        ok = AddU64Checked(addr, magnitude, out);
    }

    if (!ok)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_AddressExpr_ReadPointer(Memmy_Process *process, Memmy_Addr addr, Memmy_Addr *out,
                                                  Memmy_Error *error)
{
    if (process == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("missing selected process for pointer dereference"));
        return Memmy_Status_InvalidArgument;
    }

    U64 pointer_size = 0;
    if (process->pointer_width == Memmy_PointerWidth_32)
    {
        pointer_size = 4;
    }
    else if (process->pointer_width == Memmy_PointerWidth_64)
    {
        pointer_size = 8;
    }
    else
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("address"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }

    U8 bytes[8] = {0};
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Process_Read(process, addr, bytes, pointer_size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != pointer_size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("address"),
                        String8_Lit("pointer read returned too few bytes"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }

    Memmy_Addr value = 0;
    for (U64 i = 0; i < pointer_size; i++)
    {
        value |= ((Memmy_Addr)bytes[i]) << (i * 8);
    }

    *out = value;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_AddressExpr_Resolve(Memmy_Process *process, Memmy_AddressExpr *expr, Memmy_Addr *out,
                                       Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("missing address expression or output address"));
        return Memmy_Status_InvalidArgument;
    }

    Memmy_Addr addr = 0;
    if (expr->base_kind == Memmy_AddressExprBaseKind_Absolute)
    {
        addr = expr->absolute;
    }
    else if (expr->base_kind == Memmy_AddressExprBaseKind_Target)
    {
        Memmy_Status status = Memmy_TargetExpr_ResolveModuleBase(process, &expr->target, &addr, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }
    else
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("unknown address expression base kind"));
        return Memmy_Status_InvalidArgument;
    }

    List_ForEach(Memmy_AddressOp, op, &expr->ops, link)
    {
        if (op->kind == Memmy_AddressOpKind_Add)
        {
            Memmy_Status status = Memmy_AddressExpr_ApplyOffset(addr, op->offset, &addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        else if (op->kind == Memmy_AddressOpKind_Sub)
        {
            Memmy_Status status = Memmy_AddressExpr_ApplySubOffset(addr, op->offset, &addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        else if (op->kind == Memmy_AddressOpKind_Deref || op->kind == Memmy_AddressOpKind_DerefOffset)
        {
            Memmy_Status status = Memmy_AddressExpr_ReadPointer(process, addr, &addr, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (op->kind == Memmy_AddressOpKind_DerefOffset)
            {
                status = Memmy_AddressExpr_ApplyOffset(addr, op->offset, &addr, error);
                if (status != Memmy_Status_Ok)
                {
                    return status;
                }
            }
        }
        else
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                            String8_Lit("unknown address expression operation"));
            return Memmy_Status_InvalidArgument;
        }
    }

    *out = addr;
    if (error != 0)
    {
        *error = (Memmy_Error){0};
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_MemoryExpr_ResolveAddress(Memmy_Process *process, Memmy_MemoryExpr *expr, Memmy_Addr *out,
                                             Memmy_Error *error)
{
    if (expr == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("missing memory expression or output address"));
        return Memmy_Status_InvalidArgument;
    }
    if (expr->kind != Memmy_MemoryExprKind_Address)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                        String8_Lit("memory expression is not an address expression"));
        return Memmy_Status_InvalidArgument;
    }

    return Memmy_AddressExpr_Resolve(process, &expr->address, out, error);
}
