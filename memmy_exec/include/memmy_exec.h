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

typedef struct Memmy_ExecPeekResult Memmy_ExecPeekResult;
struct Memmy_ExecPeekResult
{
    Memmy_Addr address;
    Memmy_Type type;
    Memmy_Value value;
};

typedef struct Memmy_ExecPokeResult Memmy_ExecPokeResult;
struct Memmy_ExecPokeResult
{
    Memmy_Addr address;
    Memmy_Type type;
    Memmy_Value old_value;
    Memmy_Value new_value;
};

Memmy_Status Memmy_MemoryExpr_GetRequirements(Memmy_MemoryExpr *expr, Memmy_ExecRequirements *out, Memmy_Error *error);
Memmy_Status Memmy_AddressExpr_Resolve(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_AddressExpr *expr,
                                       Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ResolveAddress(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_MemoryExpr *expr,
                                             Memmy_Addr *out, Memmy_Error *error);
Memmy_Status Memmy_RangeExpr_Resolve(Memmy_Process *process, Memmy_ModuleList *modules, Memmy_RangeExpr *expr,
                                     Memmy_Range *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePeek(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                          Memmy_MemoryExpr *expr, Memmy_ExecPeekResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePoke(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                          Memmy_MemoryExpr *expr, Memmy_ExecPokeResult *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecutePatternScan(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                                 Memmy_RegionList *regions, Memmy_MemoryExpr *expr,
                                                 Memmy_ScanResultList *out, Memmy_Error *error);
Memmy_Status Memmy_MemoryExpr_ExecuteValueScan(Arena *arena, Memmy_Process *process, Memmy_ModuleList *modules,
                                               Memmy_RegionList *regions, Memmy_MemoryExpr *expr,
                                               Memmy_ScanResultList *out, Memmy_Error *error);

#endif // MEMMY_EXEC_H
