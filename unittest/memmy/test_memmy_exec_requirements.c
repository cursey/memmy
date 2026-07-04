#include "memmy_exec.h"
#include "test_framework.h"

static void Test_ParseAndGetRequirements(Arena *arena, char *text, Memmy_ExecRequirements *out)
{
    Memmy_Error error = {0};
    Memmy_MemoryExpr expr = {0};
    AssertEq(Memmy_MemoryExpr_Parse(arena, String8_FromCStr(text), &expr, &error), Memmy_Status_Ok);

    error = (Memmy_Error){0};
    AssertEq(Memmy_MemoryExpr_GetRequirements(&expr, out, &error), Memmy_Status_Ok);
}

Test(Test_MemmyExecRequirementsAbsoluteAddressHasNoCapsOrAccess)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "0x1000", &requirements);

    AssertEq(requirements.backend_caps, 0);
    AssertEq(requirements.process_access, 0);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(!requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsAbsolutePeekRequiresRead)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "0x1000 : u32", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Read);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(!requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsAbsolutePokeRequiresReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "0x1000 : u32 = 77", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read | Memmy_BackendCap_Write);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(!requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsModuleAddressRequiresModulesAndQuery)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "<client.dll>+0x123", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_ListModules);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Query);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsModulePeekAddsRead)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "<client.dll>+0x123 : i32", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read | Memmy_BackendCap_ListModules);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsModulePokeAddsReadWrite)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "<client.dll>+0x123 : i32 = 77", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read | Memmy_BackendCap_Write | Memmy_BackendCap_ListModules);
    AssertEq(requirements.process_access,
             Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Write | Memmy_ProcessAccess_Query);
    AssertTrue(requirements.needs_external_process);
    AssertTrue(requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsWholeProcessScansRequireRegionsAndQuery)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "<game.exe!> : i32 == 42", &requirements);

    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read | Memmy_BackendCap_ListRegions);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);
    AssertTrue(!requirements.needs_external_process);
    AssertTrue(!requirements.needs_modules);
    AssertTrue(requirements.needs_regions);

    Arena_Destroy(arena);
}

Test(Test_MemmyExecRequirementsProcessNameSelectorDoesNotSetListProcs)
{
    Arena *arena = Arena_CreateDefault();
    Memmy_ExecRequirements requirements = {0};
    Test_ParseAndGetRequirements(arena, "<game.exe!client.dll>+0x123 : i32", &requirements);

    AssertEq(requirements.backend_caps & Memmy_BackendCap_ListProcs, 0);
    AssertEq(requirements.backend_caps, Memmy_BackendCap_Read | Memmy_BackendCap_ListModules);
    AssertEq(requirements.process_access, Memmy_ProcessAccess_Read | Memmy_ProcessAccess_Query);
    AssertTrue(!requirements.needs_external_process);
    AssertTrue(requirements.needs_modules);
    AssertTrue(!requirements.needs_regions);

    Arena_Destroy(arena);
}

TestSuite suite_memmy_exec_requirements =
    TestSuite_Make("Memmy Exec Requirements", TestCase_Make(Test_MemmyExecRequirementsAbsoluteAddressHasNoCapsOrAccess),
                   TestCase_Make(Test_MemmyExecRequirementsAbsolutePeekRequiresRead),
                   TestCase_Make(Test_MemmyExecRequirementsAbsolutePokeRequiresReadWrite),
                   TestCase_Make(Test_MemmyExecRequirementsModuleAddressRequiresModulesAndQuery),
                   TestCase_Make(Test_MemmyExecRequirementsModulePeekAddsRead),
                   TestCase_Make(Test_MemmyExecRequirementsModulePokeAddsReadWrite),
                   TestCase_Make(Test_MemmyExecRequirementsWholeProcessScansRequireRegionsAndQuery),
                   TestCase_Make(Test_MemmyExecRequirementsProcessNameSelectorDoesNotSetListProcs), );
