#ifndef MEMMY_EXEC_H
#define MEMMY_EXEC_H

#include "memmy.h"
#include "memmy_expr.h"

typedef struct Memmy_ExecRequirements Memmy_ExecRequirements;
struct Memmy_ExecRequirements
{
    Memmy_BackendCap backend_caps;
    Memmy_ProcessAccess process_access;
    B32 needs_external_process;
    B32 needs_modules;
    B32 needs_regions;
};

Memmy_Status Memmy_MemoryExpr_GetRequirements(Memmy_MemoryExpr *expr, Memmy_ExecRequirements *out, Memmy_Error *error);
Memmy_Status Memmy_AddressExpr_Resolve(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_AddressExpr *expr,
                                       Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ResolveAddress(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_MemoryExpr *expr,
                                             Memmy_Addr *out, Memmy_Error *error);

#endif // MEMMY_EXEC_H
