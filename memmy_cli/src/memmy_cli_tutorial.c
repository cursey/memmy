#include "memmy_cli_internal.h"

#include "base.h"

typedef U32 MemmyCli_TutorialStep;
enum
{
    MemmyCli_TutorialStep_Attach,
    MemmyCli_TutorialStep_Modules,
    MemmyCli_TutorialStep_Assignment,
    MemmyCli_TutorialStep_Variables,
    MemmyCli_TutorialStep_Range,
    MemmyCli_TutorialStep_TypedRead,
    MemmyCli_TutorialStep_PatternScan,
    MemmyCli_TutorialStep_Index,
    MemmyCli_TutorialStep_ValuePipe,
};

enum
{
    MEMMY_CLI_TUTORIAL_FIXTURE_SIZE = 64,
    MEMMY_CLI_TUTORIAL_VALUE_OFFSET = 8,
    MEMMY_CLI_TUTORIAL_MARKER_OFFSET = 32,
};

struct MemmyCli_Tutorial
{
    Arena *arena;
    U8 *fixture;
    B32 active;
    MemmyCli_TutorialStep step;
    String8 scan_variable;
    B32 captured_value;
    MemmyEval_Result captured_result;
    MemmyEval_ResultSink observer;
};

static U8 const MemmyCli_Tutorial_Marker[] = {0xde, 0xad, 0xbe, 0xef, 0x13, 0x37, 0xc0, 0xde};

static Memmy_Addr MemmyCli_Tutorial_FixtureAddress(MemmyCli_Tutorial const *tutorial)
{
    return (Memmy_Addr)(uintptr_t)tutorial->fixture;
}

static Memmy_Addr MemmyCli_Tutorial_ValueAddress(MemmyCli_Tutorial const *tutorial)
{
    return MemmyCli_Tutorial_FixtureAddress(tutorial) + MEMMY_CLI_TUTORIAL_VALUE_OFFSET;
}

static Memmy_Addr MemmyCli_Tutorial_MarkerAddress(MemmyCli_Tutorial const *tutorial)
{
    return MemmyCli_Tutorial_FixtureAddress(tutorial) + MEMMY_CLI_TUTORIAL_MARKER_OFFSET;
}

static void MemmyCli_Tutorial_Fixture_Reset(MemmyCli_Tutorial *tutorial)
{
    for (U64 i = 0; i < MEMMY_CLI_TUTORIAL_FIXTURE_SIZE; i++)
    {
        tutorial->fixture[i] = (U8)(11u + i * 37u);
    }

    tutorial->fixture[MEMMY_CLI_TUTORIAL_VALUE_OFFSET + 0] = 0x78;
    tutorial->fixture[MEMMY_CLI_TUTORIAL_VALUE_OFFSET + 1] = 0x56;
    tutorial->fixture[MEMMY_CLI_TUTORIAL_VALUE_OFFSET + 2] = 0x34;
    tutorial->fixture[MEMMY_CLI_TUTORIAL_VALUE_OFFSET + 3] = 0x12;
    Memory_Copy(tutorial->fixture + MEMMY_CLI_TUTORIAL_MARKER_OFFSET, MemmyCli_Tutorial_Marker,
                sizeof(MemmyCli_Tutorial_Marker));
}

static void MemmyCli_Tutorial_Reset(MemmyCli_Tutorial *tutorial)
{
    MemmyCli_Tutorial_Fixture_Reset(tutorial);
    tutorial->active = 1;
    tutorial->step = MemmyCli_TutorialStep_Attach;
    tutorial->scan_variable = (String8){0};
    tutorial->captured_value = 0;
    tutorial->captured_result = (MemmyEval_Result){0};
}

static String8 MemmyCli_Tutorial_Instruction(Arena *arena, MemmyCli_Tutorial const *tutorial)
{
    Memmy_Addr fixture = MemmyCli_Tutorial_FixtureAddress(tutorial);
    Memmy_Addr value = MemmyCli_Tutorial_ValueAddress(tutorial);
    if (tutorial->step == MemmyCli_TutorialStep_Attach)
    {
        return String8_PushF(arena,
                             "Tutorial 1/9: attach to Memmy\n"
                             "Select this Memmy process. Attaching clears existing variables.\n"
                             "Type:\n"
                             "  /attach %u\n",
                             Os_GetProcessId());
    }
    if (tutorial->step == MemmyCli_TutorialStep_Modules)
    {
        return String8_Lit("Tutorial 2/9: inspect modules\n"
                           "The selected process owns modules that can be used as address ranges.\n"
                           "Type:\n"
                           "  /mods\n");
    }
    if (tutorial->step == MemmyCli_TutorialStep_Assignment)
    {
        return String8_Lit("Tutorial 3/9: constants and variables\n"
                           "Expressions use normal arithmetic, and assignments persist in the REPL.\n"
                           "Type:\n"
                           "  $answer = 6 * 7\n");
    }
    if (tutorial->step == MemmyCli_TutorialStep_Variables)
    {
        return String8_Lit("Tutorial 4/9: inspect variables\n"
                           "List the bindings stored in the current REPL environment.\n"
                           "Type:\n"
                           "  /vars\n");
    }
    if (tutorial->step == MemmyCli_TutorialStep_Range)
    {
        return String8_PushF(arena,
                             "Tutorial 5/9: address ranges\n"
                             "Ranges are half-open: the start is included and the end is excluded.\n"
                             "Type:\n"
                             "  [@0x%llx..+0x40]\n",
                             (unsigned long long)fixture);
    }
    if (tutorial->step == MemmyCli_TutorialStep_TypedRead)
    {
        return String8_PushF(arena,
                             "Tutorial 6/9: typed reads\n"
                             "Read four bytes from the fixture as an unsigned 32-bit integer.\n"
                             "Type:\n"
                             "  @0x%llx as u32\n",
                             (unsigned long long)value);
    }
    if (tutorial->step == MemmyCli_TutorialStep_PatternScan)
    {
        return String8_PushF(arena,
                             "Tutorial 7/9: pattern scans\n"
                             "A byte pattern scan returns an address list. Assign it for later steps.\n"
                             "Type:\n"
                             "  $matches = [@0x%llx..+0x40]{de ad be ef 13 37 c0 de}\n",
                             (unsigned long long)fixture);
    }
    if (tutorial->step == MemmyCli_TutorialStep_Index)
    {
        return String8_PushF(arena,
                             "Tutorial 8/9: list indexing\n"
                             "Index the first address in the scan result.\n"
                             "Type:\n"
                             "  $%.*s[0]\n",
                             (int)tutorial->scan_variable.len, (char *)tutorial->scan_variable.data);
    }
    return String8_PushF(arena,
                         "Tutorial 9/9: value pipes\n"
                         "A value pipe binds its left side to $ while evaluating the right side once.\n"
                         "Type:\n"
                         "  $%.*s |> $[0]\n",
                         (int)tutorial->scan_variable.len, (char *)tutorial->scan_variable.data);
}

static String8 MemmyCli_Tutorial_Hint(Arena *arena, MemmyCli_Tutorial const *tutorial)
{
    String8 hint = {0};
    if (tutorial->step == MemmyCli_TutorialStep_Attach)
    {
        hint = String8_Lit("Hint: /attach accepts a decimal PID. Use the PID shown in the example.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_Modules)
    {
        hint = String8_Lit("Hint: /mods uses the process selected by /attach.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_Assignment)
    {
        hint = String8_Lit("Hint: assign any constant arithmetic expression whose value is 42.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_Variables)
    {
        hint = String8_Lit("Hint: /vars displays every persistent binding and its value kind.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_Range)
    {
        hint = String8_Lit("Hint: ..+0x40 makes the end exactly 64 bytes after the starting address.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_TypedRead)
    {
        hint = String8_Lit("Hint: put 'as u32' after the exact value address shown below.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_PatternScan)
    {
        hint = String8_Lit("Hint: append {hex bytes} to the exact fixture range and assign the resulting list.");
    }
    else if (tutorial->step == MemmyCli_TutorialStep_Index)
    {
        hint = String8_Lit("Hint: address lists are zero-indexed, so the first match is at index 0.");
    }
    else
    {
        hint = String8_Lit("Hint: |> makes the complete address list available as $ on its right side.");
    }

    String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
    return String8_PushF(arena, "%.*s\n\n%.*s", (int)hint.len, (char *)hint.data, (int)instruction.len,
                         (char *)instruction.data);
}

MemmyCli_Tutorial *MemmyCli_Tutorial_Create(Arena *arena)
{
    if (arena == 0)
    {
        return 0;
    }

    MemmyCli_Tutorial *tutorial = Arena_PushStruct(arena, MemmyCli_Tutorial);
    tutorial->arena = arena;
    tutorial->fixture = Arena_PushArrayNoZero(arena, U8, MEMMY_CLI_TUTORIAL_FIXTURE_SIZE);
    tutorial->observer.callback = 0;
    tutorial->observer.user_data = tutorial;
    MemmyCli_Tutorial_Fixture_Reset(tutorial);
    return tutorial;
}

Memmy_Status MemmyCli_Tutorial_Command_Run(Arena *arena, MemmyCli_Tutorial *tutorial, String8 argument, String8 *out,
                                           Memmy_Error *error)
{
    if (arena == 0 || tutorial == 0 || out == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("tutorial"),
                        String8_Lit("missing tutorial state or output"));
        return Memmy_Status_InvalidArgument;
    }

    *out = (String8){0};
    if (argument.len == 0)
    {
        if (!tutorial->active)
        {
            MemmyCli_Tutorial_Reset(tutorial);
            String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
            *out = String8_PushF(arena, "Welcome to the interactive Memmy tutorial.\n\n%.*s", (int)instruction.len,
                                 (char *)instruction.data);
        }
        else
        {
            *out = MemmyCli_Tutorial_Instruction(arena, tutorial);
        }
        return Memmy_Status_Ok;
    }
    if (String8_Eq(argument, String8_Lit("hint")))
    {
        if (!tutorial->active)
        {
            Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("tutorial"),
                            String8_Lit("tutorial is not active"));
            return Memmy_Status_InvalidArgument;
        }
        *out = MemmyCli_Tutorial_Hint(arena, tutorial);
        return Memmy_Status_Ok;
    }
    if (String8_Eq(argument, String8_Lit("restart")))
    {
        MemmyCli_Tutorial_Reset(tutorial);
        String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
        *out = String8_PushF(arena, "Tutorial restarted.\n\n%.*s", (int)instruction.len, (char *)instruction.data);
        return Memmy_Status_Ok;
    }
    if (String8_Eq(argument, String8_Lit("stop")))
    {
        B32 was_active = tutorial->active;
        tutorial->active = 0;
        *out = was_active ? String8_Lit("Tutorial stopped.\n") : String8_Lit("Tutorial is not active.\n");
        return Memmy_Status_Ok;
    }

    Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("tutorial"),
                    String8_Lit("expected hint, restart, or stop"));
    if (error != 0)
    {
        error->input = argument;
    }
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status MemmyCli_Tutorial_Result_Push(void *user_data, MemmyEval_Result const *result)
{
    MemmyCli_Tutorial *tutorial = (MemmyCli_Tutorial *)user_data;
    if (result->kind == MemmyEval_ResultKind_Value || result->kind == MemmyEval_ResultKind_Read ||
        result->kind == MemmyEval_ResultKind_AddressList)
    {
        tutorial->captured_value = 1;
        tutorial->captured_result = *result;
    }
    return Memmy_Status_Ok;
}

MemmyEval_ResultSink const *MemmyCli_Tutorial_Statement_Begin(MemmyCli_Tutorial *tutorial)
{
    if (tutorial == 0 || !tutorial->active)
    {
        return 0;
    }

    tutorial->captured_value = 0;
    tutorial->captured_result = (MemmyEval_Result){0};
    tutorial->observer.callback = MemmyCli_Tutorial_Result_Push;
    return &tutorial->observer;
}

static B32 MemmyCli_Tutorial_Value_IsAddress(MemmyCli_Tutorial const *tutorial, MemmyEval_Value value)
{
    return value.kind == MemmyEval_ValueKind_Address && value.address == MemmyCli_Tutorial_MarkerAddress(tutorial);
}

static B32 MemmyCli_Tutorial_Pattern_IsMarker(Arena *arena, MemmyAst_Node const *scan)
{
    Scratch scratch = Scratch_Begin(&arena, 1);
    Memmy_Pattern pattern = {0};
    if (scan == 0 || scan->kind != MemmyAst_NodeKind_PatternScan ||
        Memmy_Pattern_Parse(scratch.arena, scan->pattern, Memmy_PatternParseFlag_AllowWildcards, &pattern, 0) !=
            Memmy_Status_Ok ||
        pattern.count != sizeof(MemmyCli_Tutorial_Marker))
    {
        Scratch_End(scratch);
        return 0;
    }
    for (U64 i = 0; i < pattern.count; i++)
    {
        if (pattern.bytes[i].wildcard || pattern.bytes[i].value != MemmyCli_Tutorial_Marker[i])
        {
            Scratch_End(scratch);
            return 0;
        }
    }
    Scratch_End(scratch);
    return 1;
}

static B32 MemmyCli_Tutorial_Node_IsVariable(MemmyAst_Node const *node, String8 name)
{
    return node != 0 && node->kind == MemmyAst_NodeKind_Variable && String8_Eq(node->name, name);
}

static B32 MemmyCli_Tutorial_Node_IsCurrentIndex(MemmyAst_Node const *node)
{
    return node != 0 && node->kind == MemmyAst_NodeKind_Index && node->lhs != 0 &&
           node->lhs->kind == MemmyAst_NodeKind_CurrentItem;
}

static B32 MemmyCli_Tutorial_Range_IsFixture(MemmyCli_Tutorial const *tutorial, MemmyEval_Value value)
{
    Memmy_Addr start = MemmyCli_Tutorial_FixtureAddress(tutorial);
    return value.kind == MemmyEval_ValueKind_Range && value.range.start == start &&
           value.range.end == start + MEMMY_CLI_TUTORIAL_FIXTURE_SIZE;
}

static B32 MemmyCli_Tutorial_ScanRange_IsFixture(Arena *arena, MemmyCli_Tutorial const *tutorial, MemmyEval_Env *env,
                                                 MemmyAst_Node const *scan)
{
    Arena *conflicts[] = {arena, tutorial->arena};
    Scratch scratch = Scratch_Begin(conflicts, ArrayCount(conflicts));
    MemmyEval_Value value = {0};
    B32 result = scan != 0 && scan->lhs != 0 &&
                 MemmyEval_Expr_Eval(scratch.arena, env, scan->lhs, &value, 0) == Memmy_Status_Ok &&
                 MemmyCli_Tutorial_Range_IsFixture(tutorial, value);
    Scratch_End(scratch);
    return result;
}

static B32 MemmyCli_Tutorial_ScanVariable_Exists(Arena *arena, MemmyCli_Tutorial const *tutorial, MemmyEval_Env *env)
{
    Arena *conflicts[] = {arena, tutorial->arena};
    Scratch scratch = Scratch_Begin(conflicts, ArrayCount(conflicts));
    MemmyEval_Value value = {0};
    B32 result = tutorial->scan_variable.len != 0 &&
                 MemmyEval_Env_Find(scratch.arena, env, tutorial->scan_variable, &value) == Memmy_Status_Ok;
    Scratch_End(scratch);
    return result;
}

static B32 MemmyCli_Tutorial_Statement_Succeeds(Arena *arena, MemmyCli_Tutorial *tutorial,
                                                MemmyAst_Statement const *statement, MemmyEval_Env *env)
{
    MemmyEval_Value value = tutorial->captured_result.value;
    if (tutorial->step == MemmyCli_TutorialStep_Modules)
    {
        return statement->kind == MemmyAst_NodeKind_Command && statement->command_kind == MemmyAst_CommandKind_Mods &&
               statement->command_arg.len == 0;
    }
    if (tutorial->step == MemmyCli_TutorialStep_Assignment)
    {
        return statement->kind == MemmyAst_NodeKind_Assignment && tutorial->captured_value &&
               value.kind == MemmyEval_ValueKind_Const && value.constant == 42;
    }
    if (tutorial->step == MemmyCli_TutorialStep_Variables)
    {
        return statement->kind == MemmyAst_NodeKind_Command && statement->command_kind == MemmyAst_CommandKind_Vars;
    }
    if (tutorial->step == MemmyCli_TutorialStep_Range)
    {
        return tutorial->captured_value && MemmyCli_Tutorial_Range_IsFixture(tutorial, value);
    }
    if (tutorial->step == MemmyCli_TutorialStep_TypedRead)
    {
        return statement->expr != 0 && statement->expr->kind == MemmyAst_NodeKind_TypedRead &&
               tutorial->captured_value && value.kind == MemmyEval_ValueKind_TypedValue &&
               value.address == MemmyCli_Tutorial_ValueAddress(tutorial) &&
               value.typed_value.type.kind == Memmy_TypeKind_U32 && value.typed_value.bytes.len == 4 &&
               value.typed_value.bytes.data[0] == 0x78 && value.typed_value.bytes.data[1] == 0x56 &&
               value.typed_value.bytes.data[2] == 0x34 && value.typed_value.bytes.data[3] == 0x12;
    }
    if (tutorial->step == MemmyCli_TutorialStep_PatternScan)
    {
        MemmyAst_Node const *scan = statement->assignment_value;
        if (statement->kind != MemmyAst_NodeKind_Assignment || !tutorial->captured_value ||
            !MemmyCli_Tutorial_Pattern_IsMarker(arena, scan) ||
            !MemmyCli_Tutorial_ScanRange_IsFixture(arena, tutorial, env, scan) ||
            value.kind != MemmyEval_ValueKind_AddressList || value.address_count != 1 ||
            value.addresses[0] != MemmyCli_Tutorial_MarkerAddress(tutorial))
        {
            return 0;
        }
        tutorial->scan_variable = String8_Copy(tutorial->arena, statement->assignment_name);
        return 1;
    }
    if (tutorial->step == MemmyCli_TutorialStep_Index)
    {
        return statement->expr != 0 && statement->expr->kind == MemmyAst_NodeKind_Index &&
               MemmyCli_Tutorial_Node_IsVariable(statement->expr->lhs, tutorial->scan_variable) &&
               tutorial->captured_value && MemmyCli_Tutorial_Value_IsAddress(tutorial, value);
    }
    if (tutorial->step == MemmyCli_TutorialStep_ValuePipe)
    {
        return statement->expr != 0 && statement->expr->kind == MemmyAst_NodeKind_ValuePipe &&
               MemmyCli_Tutorial_Node_IsVariable(statement->expr->lhs, tutorial->scan_variable) &&
               MemmyCli_Tutorial_Node_IsCurrentIndex(statement->expr->rhs) && tutorial->captured_value &&
               MemmyCli_Tutorial_Value_IsAddress(tutorial, value);
    }
    return 0;
}

String8 MemmyCli_Tutorial_Statement_End(Arena *arena, MemmyCli_Tutorial *tutorial, MemmyAst_Statement const *statement,
                                        Memmy_Status status, B32 has_attached_process, U32 attached_pid,
                                        MemmyEval_Env *env)
{
    if (arena == 0 || tutorial == 0 || !tutorial->active || statement == 0)
    {
        return (String8){0};
    }

    if (tutorial->step != MemmyCli_TutorialStep_Attach && (!has_attached_process || attached_pid != Os_GetProcessId()))
    {
        tutorial->step = MemmyCli_TutorialStep_Attach;
        tutorial->scan_variable = (String8){0};
        String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
        return String8_PushF(arena, "The selected process changed. Returning to the attach lesson.\n\n%.*s",
                             (int)instruction.len, (char *)instruction.data);
    }

    if ((tutorial->step == MemmyCli_TutorialStep_Index || tutorial->step == MemmyCli_TutorialStep_ValuePipe) &&
        !MemmyCli_Tutorial_ScanVariable_Exists(arena, tutorial, env))
    {
        tutorial->step = MemmyCli_TutorialStep_PatternScan;
        tutorial->scan_variable = (String8){0};
        String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
        return String8_PushF(arena, "The saved scan variable was cleared. Returning to the scan lesson.\n\n%.*s",
                             (int)instruction.len, (char *)instruction.data);
    }

    if (status != Memmy_Status_Ok)
    {
        return (String8){0};
    }

    if (tutorial->step == MemmyCli_TutorialStep_Attach)
    {
        if (statement->kind != MemmyAst_NodeKind_Command || statement->command_kind != MemmyAst_CommandKind_Attach ||
            !has_attached_process || attached_pid != Os_GetProcessId())
        {
            return (String8){0};
        }
    }
    else if (!MemmyCli_Tutorial_Statement_Succeeds(arena, tutorial, statement, env))
    {
        return (String8){0};
    }

    if (tutorial->step == MemmyCli_TutorialStep_ValuePipe)
    {
        tutorial->active = 0;
        return String8_Lit("Tutorial complete! You attached to a process, built values and ranges, read memory, "
                           "scanned bytes, indexed results, and used a value pipe.\n"
                           "Use /help for the full command overview and keep the DSL pocket reference nearby.\n");
    }

    tutorial->step++;
    String8 instruction = MemmyCli_Tutorial_Instruction(arena, tutorial);
    return String8_PushF(arena, "Step complete.\n\n%.*s", (int)instruction.len, (char *)instruction.data);
}
