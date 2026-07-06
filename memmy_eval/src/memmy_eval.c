#include "memmy_eval.h"

#include "base_checked.h"
#include "base_hash.h"
#include "base_list.h"
#include "base_memory.h"

#define MEMMY_EVAL_STRING_READ_MAX 4096
#define MEMMY_EVAL_STRING_READ_CHUNK_SIZE 256

struct Memmy_EvalBinding
{
    HashLink hash;
    String8 name;
    Memmy_EvalValue value;
};

typedef struct Memmy_EvalModuleResolver Memmy_EvalModuleResolver;
struct Memmy_EvalModuleResolver
{
    String8 name;
    Memmy_Module match;
    U64 match_count;
    Memmy_Error *error;
};

typedef struct Memmy_EvalScanResultNode Memmy_EvalScanResultNode;
struct Memmy_EvalScanResultNode
{
    ListLink link;
    Memmy_Addr address;
};

typedef struct Memmy_EvalAddressNode Memmy_EvalAddressNode;
struct Memmy_EvalAddressNode
{
    ListLink link;
    Memmy_Addr address;
};

typedef struct Memmy_EvalRangeNode Memmy_EvalRangeNode;
struct Memmy_EvalRangeNode
{
    ListLink link;
    Memmy_Range range;
};

typedef struct Memmy_EvalScanCollector Memmy_EvalScanCollector;
struct Memmy_EvalScanCollector
{
    Arena *arena;
    List addresses; // Memmy_EvalScanResultNode
};

typedef struct Memmy_EvalByteReader Memmy_EvalByteReader;
struct Memmy_EvalByteReader
{
    Memmy_Process *process;
    Memmy_Addr address;
    U64 offset;
    U8 buffer[MEMMY_EVAL_STRING_READ_CHUNK_SIZE];
    U64 pos;
    U64 count;
    Memmy_Status terminal_status;
};

typedef struct Memmy_EvalProcessEmitter Memmy_EvalProcessEmitter;
struct Memmy_EvalProcessEmitter
{
    Memmy_EvalResultSink *sink;
    String8 filter;
};

typedef struct Memmy_EvalModuleEmitter Memmy_EvalModuleEmitter;
struct Memmy_EvalModuleEmitter
{
    Memmy_EvalResultSink *sink;
    String8 filter;
};

typedef struct Memmy_EvalRegionEmitter Memmy_EvalRegionEmitter;
struct Memmy_EvalRegionEmitter
{
    Memmy_EvalResultSink *sink;
};

typedef struct Memmy_EvalOpenProcess Memmy_EvalOpenProcess;
struct Memmy_EvalOpenProcess
{
    ListLink link;
    Memmy_Process *process;
};

typedef struct Memmy_EvalExec Memmy_EvalExec;
struct Memmy_EvalExec
{
    Memmy_EvalEnv *env;
    Arena *transient_arena;
    List open_processes; // Memmy_EvalOpenProcess
    B32 has_current_item;
    Memmy_EvalValue current_item;
};

static Memmy_Status Memmy_EvalExprWithContext(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out,
                                              Memmy_Error *error);
static Memmy_Status Memmy_EvalStatementWithContext(Memmy_EvalExec *exec, Memmy_AstStatement *statement,
                                                   Memmy_EvalResultSink *sink, Memmy_Error *error);
static Memmy_Status Memmy_Eval_Command(Memmy_EvalExec *exec, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                       Memmy_Error *error);

static void Memmy_EvalError(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    Memmy_Error_Set(error, status, context, message);
}

static B32 Memmy_EvalBinding_Eq(void *link, void *ctx)
{
    Memmy_EvalBinding *binding = ContainerOf((HashLink *)link, Memmy_EvalBinding, hash);
    String8 *name = (String8 *)ctx;
    return String8_Eq(binding->name, *name);
}

static Memmy_EvalBinding *Memmy_EvalEnv_FindBinding(Memmy_EvalEnv *env, String8 name)
{
    U64 hash = Hash_Fnv1a(name.data, name.len);
    HashLink *link = HashMap_Find(&env->bindings, hash, Memmy_EvalBinding_Eq, &name);
    return link != 0 ? ContainerOf(link, Memmy_EvalBinding, hash) : 0;
}

static Memmy_EvalValue Memmy_EvalValue_Copy(Arena *arena, Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        value.typed_value.bytes = String8_Copy(arena, value.typed_value.bytes);
        value.old_typed_value.bytes = String8_Copy(arena, value.old_typed_value.bytes);
    }
    if (value.kind == Memmy_EvalValueKind_AddressList && value.address_count != 0)
    {
        Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, value.address_count);
        Memory_Copy(addresses, value.addresses, sizeof(addresses[0]) * value.address_count);
        value.addresses = addresses;
    }
    if (value.kind == Memmy_EvalValueKind_RangeList && value.range_count != 0)
    {
        Memmy_Range *ranges = Arena_PushArrayNoZero(arena, Memmy_Range, value.range_count);
        Memory_Copy(ranges, value.ranges, sizeof(ranges[0]) * value.range_count);
        value.ranges = ranges;
    }
    return value;
}

static void Memmy_EvalExec_Close(Memmy_EvalExec *exec)
{
    if (exec == 0)
    {
        return;
    }

    List_ForEach(Memmy_EvalOpenProcess, node, &exec->open_processes, link)
    {
        Memmy_Process_Close(node->process);
    }
}

static Memmy_Status Memmy_EvalExec_OpenProcess(Memmy_EvalExec *exec, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    if (exec == 0 || exec->env == 0 || out == 0)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("process"),
                        String8_Lit("missing eval execution context"));
        return Memmy_Status_InvalidArgument;
    }

    List_ForEach(Memmy_EvalOpenProcess, node, &exec->open_processes, link)
    {
        if (node->process != 0 && node->process->pid == pid)
        {
            *out = node->process;
            return Memmy_Status_Ok;
        }
    }

    Memmy_Process *process = 0;
    Arena *arena = exec->transient_arena != 0 ? exec->transient_arena : exec->env->arena;
    Memmy_Status status = Memmy_Process_Open(arena, pid, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_EvalOpenProcess *node = Arena_PushStruct(arena, Memmy_EvalOpenProcess);
    node->process = process;
    List_PushBack(&exec->open_processes, &node->link);
    *out = process;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_RequireProcess(Memmy_EvalExec *exec, Memmy_EvalValue *value, String8 context,
                                              Memmy_Process **out, Memmy_Error *error)
{
    (void)value;
    U32 pid = 0;
    if (exec != 0 && exec->env != 0 && exec->env->has_default_process)
    {
        pid = exec->env->default_pid;
    }
    else
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, context, String8_Lit("missing selected process"));
        return Memmy_Status_InvalidArgument;
    }

    return Memmy_EvalExec_OpenProcess(exec, pid, out, error);
}

static void Memmy_EvalResult_Push(Memmy_EvalResultSink *sink, Memmy_EvalResult result)
{
    if (sink != 0 && sink->push != 0)
    {
        sink->push(sink, result);
    }
}

static B32 Memmy_EvalValue_IsIntegerTyped(Memmy_EvalValue *value)
{
    if (value->kind != Memmy_EvalValueKind_TypedValue)
    {
        return 0;
    }

    Memmy_TypeKind kind = value->typed_value.type.kind;
    return kind == Memmy_TypeKind_U8 || kind == Memmy_TypeKind_I8 || kind == Memmy_TypeKind_U16 ||
           kind == Memmy_TypeKind_I16 || kind == Memmy_TypeKind_U32 || kind == Memmy_TypeKind_I32 ||
           kind == Memmy_TypeKind_U64 || kind == Memmy_TypeKind_I64 || kind == Memmy_TypeKind_Ptr;
}

static Memmy_Status Memmy_EvalValue_AsConst(Memmy_EvalValue *value, I64 *out, Memmy_Error *error)
{
    if (value->kind == Memmy_EvalValueKind_Const)
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }
    if (Memmy_EvalValue_IsIntegerTyped(value))
    {
        *out = value->constant;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                    String8_Lit("expected constant integer value"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_EvalValue_AsAddress(Memmy_EvalValue *value, Memmy_Addr *out, Memmy_Error *error)
{
    if (value->kind == Memmy_EvalValueKind_Address)
    {
        *out = value->address;
        return Memmy_Status_Ok;
    }
    if (value->kind == Memmy_EvalValueKind_Range)
    {
        *out = value->range.start;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"), String8_Lit("expected address value"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Eval_ParseType(String8 type_name, Memmy_Type *out, Memmy_Error *error)
{
    Memmy_Status status = Memmy_Type_Parse(type_name, out, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_TypeSize(Memmy_Process *process, Memmy_Type type, U64 *out, Memmy_Error *error)
{
    if (type.kind == Memmy_TypeKind_Ptr)
    {
        if (process->pointer_width == Memmy_PointerWidth_32)
        {
            *out = 4;
            return Memmy_Status_Ok;
        }
        if (process->pointer_width == Memmy_PointerWidth_64)
        {
            *out = 8;
            return Memmy_Status_Ok;
        }
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }
    if (type.fixed_size == 0)
    {
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("type"),
                        String8_Lit("variable-width typed reads are not supported"));
        return Memmy_Status_Unsupported;
    }

    *out = type.fixed_size;
    return Memmy_Status_Ok;
}

static B32 Memmy_Eval_IsPrintableCodepoint(U32 cp)
{
    return (cp < 0x80 && Char8_IsPrint((U8)cp)) || (cp >= 0x80 && cp <= 0x10ffff);
}

static Memmy_Status Memmy_EvalByteReader_Read(Memmy_EvalByteReader *reader, U8 *out, Memmy_Error *error)
{
    if (reader->pos == reader->count)
    {
        if (reader->terminal_status != Memmy_Status_Ok)
        {
            return reader->terminal_status;
        }

        U64 bytes_read = 0;
        Memmy_Status status = Memmy_Process_Read(reader->process, reader->address + reader->offset, reader->buffer,
                                                 sizeof(reader->buffer), &bytes_read, error);
        if (status != Memmy_Status_Ok && bytes_read == 0)
        {
            return status;
        }
        if (status != Memmy_Status_Ok || bytes_read != sizeof(reader->buffer))
        {
            reader->terminal_status = status != Memmy_Status_Ok ? status : Memmy_Status_PartialRead;
        }
        reader->offset += bytes_read;
        reader->pos = 0;
        reader->count = bytes_read;
    }

    *out = reader->buffer[reader->pos++];
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ReadStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                       Memmy_Value *out, Memmy_Error *error)
{
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, MEMMY_EVAL_STRING_READ_MAX);
    U64 len = 0;
    Memmy_EvalByteReader reader = {
        .process = process,
        .address = address,
    };

    while (len < MEMMY_EVAL_STRING_READ_MAX)
    {
        U8 sequence[4];
        U64 need = 0;
        U32 cp = 0;
        Memmy_Status status = Memmy_EvalByteReader_Read(&reader, &sequence[0], error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        U8 first = sequence[0];
        if (first == 0)
        {
            break;
        }
        if (first < 0x80)
        {
            cp = first;
            need = 1;
        }
        else if (first >= 0xc2 && first <= 0xdf)
        {
            cp = first & 0x1f;
            need = 2;
        }
        else if (first >= 0xe0 && first <= 0xef)
        {
            cp = first & 0x0f;
            need = 3;
        }
        else if (first >= 0xf0 && first <= 0xf4)
        {
            cp = first & 0x07;
            need = 4;
        }
        else
        {
            break;
        }
        if (len + need > MEMMY_EVAL_STRING_READ_MAX)
        {
            break;
        }

        B32 valid = 1;
        for (U64 i = 1; i < need; i++)
        {
            status = Memmy_EvalByteReader_Read(&reader, &sequence[i], error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if ((sequence[i] & 0xc0) != 0x80)
            {
                valid = 0;
                break;
            }
            cp = (cp << 6) | (sequence[i] & 0x3f);
        }
        if (!valid || (need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) ||
            !Memmy_Eval_IsPrintableCodepoint(cp))
        {
            break;
        }

        for (U64 i = 0; i < need; i++)
        {
            buffer[len++] = sequence[i];
        }
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(buffer, len)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ReadWStr(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                        Memmy_Value *out, Memmy_Error *error)
{
    U64 max_size = MEMMY_EVAL_STRING_READ_MAX * 2;
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, max_size);
    U64 len = 0;
    U64 offset = 0;
    U16 pending_high = 0;

    while (offset < max_size)
    {
        U8 chunk[MEMMY_EVAL_STRING_READ_CHUNK_SIZE];
        U64 remaining = max_size - offset;
        U64 size = Min(remaining, (U64)sizeof(chunk));
        if ((size & 1) != 0)
        {
            size--;
        }

        U64 bytes_read = 0;
        Memmy_Status status = Memmy_Process_Read(process, address + offset, chunk, size, &bytes_read, error);
        if (status != Memmy_Status_Ok && bytes_read == 0)
        {
            return status;
        }
        bytes_read &= ~1ull;
        offset += bytes_read;

        B32 stopped = 0;
        for (U64 i = 0; i < bytes_read; i += 2)
        {
            U16 unit = (U16)(chunk[i] | (chunk[i + 1] << 8));
            if (pending_high != 0)
            {
                if (unit < 0xdc00 || unit > 0xdfff)
                {
                    stopped = 1;
                    break;
                }
                if (len + 4 > max_size)
                {
                    stopped = 1;
                    break;
                }
                buffer[len++] = (U8)pending_high;
                buffer[len++] = (U8)(pending_high >> 8);
                buffer[len++] = (U8)unit;
                buffer[len++] = (U8)(unit >> 8);
                pending_high = 0;
                continue;
            }

            if (unit == 0 || (unit >= 0xdc00 && unit <= 0xdfff))
            {
                stopped = 1;
                break;
            }
            if (unit >= 0xd800 && unit <= 0xdbff)
            {
                pending_high = unit;
                continue;
            }
            if (!Memmy_Eval_IsPrintableCodepoint(unit))
            {
                stopped = 1;
                break;
            }
            buffer[len++] = (U8)unit;
            buffer[len++] = (U8)(unit >> 8);
        }

        if (stopped || len == max_size)
        {
            break;
        }
        if (status != Memmy_Status_Ok || bytes_read != size)
        {
            Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("read"), String8_Lit("partial string read"));
            if (error != 0)
            {
                error->byte_count = bytes_read;
            }
            return Memmy_Status_PartialRead;
        }
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(buffer, len)};
    return Memmy_Status_Ok;
}

static I64 Memmy_Eval_IntegerFromBytes(Memmy_Value value)
{
    U64 raw = 0;
    U64 size = Min(value.bytes.len, (U64)sizeof(raw));
    for (U64 i = 0; i < size; i++)
    {
        raw |= ((U64)value.bytes.data[i]) << (i * 8);
    }

    switch (value.type.kind)
    {
    case Memmy_TypeKind_I8:
        return (I8)raw;
    case Memmy_TypeKind_I16:
        return (I16)raw;
    case Memmy_TypeKind_I32:
        return (I32)raw;
    case Memmy_TypeKind_I64:
        return (I64)raw;
    default:
        return (I64)raw;
    }
}

static Memmy_Status Memmy_Eval_ReadValue(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Type type,
                                         Memmy_Value *out, Memmy_Error *error)
{
    if (type.kind == Memmy_TypeKind_Str)
    {
        return Memmy_Eval_ReadStr(arena, process, address, type, out, error);
    }
    if (type.kind == Memmy_TypeKind_WStr)
    {
        return Memmy_Eval_ReadWStr(arena, process, address, type, out, error);
    }

    U64 size = 0;
    Memmy_Status status = Memmy_Eval_TypeSize(process, type, &size, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U8 *bytes = Arena_PushArrayNoZero(arena, U8, size);
    U64 bytes_read = 0;
    status = Memmy_Process_Read(process, address, bytes, size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != size)
    {
        Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("read"), String8_Lit("partial typed read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }

    *out = (Memmy_Value){.type = type, .bytes = String8_Make(bytes, size)};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ParseValue(Memmy_EvalExec *exec, Memmy_Process *process, Memmy_Type type,
                                          String8 value_text, Memmy_Value *out, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    String8 parse_text = value_text;
    Scratch scratch = {0};
    B32 using_scratch = 0;
    if (Memmy_EvalValue_IsIntegerTyped(
            &(Memmy_EvalValue){.kind = Memmy_EvalValueKind_TypedValue, .typed_value = {.type = type}}) &&
        String8_FindChar(value_text, '$', 0) != STRING8_NPOS)
    {
        scratch = Scratch_Begin(&env->arena, 1);
        using_scratch = 1;
        Memmy_AstNode *expr = 0;
        Memmy_AstDiagnostic diagnostic = {0};
        Memmy_AstStatus ast_status = Memmy_Ast_ParseExpr(scratch.arena, value_text, &expr, &diagnostic);
        if (ast_status != Memmy_AstStatus_Ok)
        {
            Scratch_End(scratch);
            Memmy_EvalError(error, Memmy_Status_ParseError, String8_Lit("value"),
                            String8_Lit("invalid value expression"));
            return Memmy_Status_ParseError;
        }

        Memmy_EvalValue value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr, &value, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        I64 constant = 0;
        status = Memmy_EvalValue_AsConst(&value, &constant, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        parse_text = String8_PushF(scratch.arena, "%lld", constant);
    }

    Memmy_Status status = Memmy_Value_Parse(env->arena, type, process->pointer_width, parse_text, out, error);
    if (using_scratch)
    {
        Scratch_End(scratch);
    }
    return status;
}

static Memmy_Status Memmy_EvalScanCollector_Push(void *user_data, Memmy_Addr address)
{
    Memmy_EvalScanCollector *collector = (Memmy_EvalScanCollector *)user_data;
    Memmy_EvalScanResultNode *node = Arena_PushStruct(collector->arena, Memmy_EvalScanResultNode);
    node->address = address;
    List_PushBack(&collector->addresses, &node->link);
    return Memmy_Status_Ok;
}

static Memmy_ReferenceScanMode Memmy_Eval_ReferenceScanMode(Memmy_AstReferenceMode mode)
{
    switch (mode)
    {
    case Memmy_AstReferenceMode_Ptr:
        return Memmy_ReferenceScanMode_Ptr;
    case Memmy_AstReferenceMode_Rel32:
        return Memmy_ReferenceScanMode_Rel32;
    case Memmy_AstReferenceMode_Any:
        return Memmy_ReferenceScanMode_Any;
    default:
        return Memmy_ReferenceScanMode_Any;
    }
}

static Memmy_EvalValue Memmy_Eval_AddressListFromCollector(Arena *arena, Memmy_EvalScanCollector *collector)
{
    Memmy_Addr *addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, collector->addresses.count);
    U64 index = 0;
    List_ForEach(Memmy_EvalScanResultNode, node, &collector->addresses, link)
    {
        addresses[index++] = node->address;
    }

    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_AddressList,
        .addresses = addresses,
        .address_count = collector->addresses.count,
    };
}

static void Memmy_EvalAddressList_Push(Arena *arena, List *list, Memmy_Addr address)
{
    Memmy_EvalAddressNode *node = Arena_PushStruct(arena, Memmy_EvalAddressNode);
    node->address = address;
    List_PushBack(list, &node->link);
}

static void Memmy_EvalRangeList_Push(Arena *arena, List *list, Memmy_Range range)
{
    Memmy_EvalRangeNode *node = Arena_PushStruct(arena, Memmy_EvalRangeNode);
    node->range = range;
    List_PushBack(list, &node->link);
}

static Memmy_EvalValue Memmy_Eval_AddressListFromList(Arena *arena, List *list)
{
    Memmy_Addr *addresses = 0;
    if (list->count != 0)
    {
        addresses = Arena_PushArrayNoZero(arena, Memmy_Addr, list->count);
    }
    U64 index = 0;
    List_ForEach(Memmy_EvalAddressNode, node, list, link)
    {
        addresses[index++] = node->address;
    }
    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_AddressList,
        .addresses = addresses,
        .address_count = list->count,
    };
}

static Memmy_EvalValue Memmy_Eval_RangeListFromList(Arena *arena, List *list)
{
    Memmy_Range *ranges = 0;
    if (list->count != 0)
    {
        ranges = Arena_PushArrayNoZero(arena, Memmy_Range, list->count);
    }
    U64 index = 0;
    List_ForEach(Memmy_EvalRangeNode, node, list, link)
    {
        ranges[index++] = node->range;
    }
    return (Memmy_EvalValue){
        .kind = Memmy_EvalValueKind_RangeList,
        .ranges = ranges,
        .range_count = list->count,
    };
}

static Memmy_Status Memmy_EvalTransform_ListKindForValue(Memmy_EvalValue value, Memmy_EvalValueKind *out_kind,
                                                         Memmy_Error *error)
{
    if (value.kind == Memmy_EvalValueKind_Address || value.kind == Memmy_EvalValueKind_AddressList)
    {
        *out_kind = Memmy_EvalValueKind_AddressList;
        return Memmy_Status_Ok;
    }
    if (value.kind == Memmy_EvalValueKind_Range || value.kind == Memmy_EvalValueKind_RangeList)
    {
        *out_kind = Memmy_EvalValueKind_RangeList;
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                    String8_Lit("transform expression must produce address or range values"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_Eval_AddConst(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!AddI64Checked(a, b, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_SubConst(I64 a, I64 b, I64 *out, Memmy_Error *error)
{
    if (!SubI64Checked(a, b, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_AddressAddConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
{
    if (!AddI64ToU64Checked(address, constant, out))
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_AddressSubConst(Memmy_Addr address, I64 constant, Memmy_Addr *out, Memmy_Error *error)
{
    B32 ok = 0;
    if (constant >= 0)
    {
        ok = SubU64Checked(address, (U64)constant, out);
    }
    else
    {
        U64 magnitude = (U64)(-(constant + 1)) + 1;
        ok = AddU64Checked(address, magnitude, out);
    }

    if (!ok)
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("address"),
                        String8_Lit("address arithmetic overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ApplyBinary(Memmy_AstConstOp op, Memmy_EvalValue lhs, Memmy_EvalValue rhs,
                                           Memmy_EvalValue *out, Memmy_Error *error)
{
    if ((op == Memmy_AstConstOp_Add || op == Memmy_AstConstOp_Sub) &&
        (lhs.kind == Memmy_EvalValueKind_Address || lhs.kind == Memmy_EvalValueKind_Range))
    {
        I64 constant = 0;
        Memmy_Status status = Memmy_EvalValue_AsConst(&rhs, &constant, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&lhs, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = op == Memmy_AstConstOp_Add ? Memmy_Eval_AddressAddConst(address, constant, &address, error)
                                            : Memmy_Eval_AddressSubConst(address, constant, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_Address,
            .address = address,
        };
        return Memmy_Status_Ok;
    }

    I64 a = 0;
    I64 b = 0;
    Memmy_Status status = Memmy_EvalValue_AsConst(&lhs, &a, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    status = Memmy_EvalValue_AsConst(&rhs, &b, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    I64 result = 0;
    switch (op)
    {
    case Memmy_AstConstOp_Add:
        status = Memmy_Eval_AddConst(a, b, &result, error);
        break;
    case Memmy_AstConstOp_Sub:
        status = Memmy_Eval_SubConst(a, b, &result, error);
        break;
    case Memmy_AstConstOp_Mul:
        if (!MulI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case Memmy_AstConstOp_Div:
        if (b == 0)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"), String8_Lit("division by zero"));
            return Memmy_Status_InvalidArgument;
        }
        if (!DivI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    case Memmy_AstConstOp_Mod:
        if (b == 0)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"), String8_Lit("modulo by zero"));
            return Memmy_Status_InvalidArgument;
        }
        if (!ModI64Checked(a, b, &result))
        {
            status = Memmy_Status_Overflow;
        }
        break;
    default:
        status = Memmy_Status_InvalidArgument;
        break;
    }

    if (status == Memmy_Status_Overflow)
    {
        Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"), String8_Lit("constant arithmetic overflow"));
        return status;
    }
    if (status != Memmy_Status_Ok)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("expr"),
                        String8_Lit("unsupported arithmetic expression"));
        return status;
    }

    *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = result};
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ResolveTargetProcess(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_Process **out,
                                                    Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    (void)target;
    if (!env->has_default_process)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("target"),
                        String8_Lit("missing selected process for target"));
        return Memmy_Status_InvalidArgument;
    }
    return Memmy_EvalExec_OpenProcess(exec, env->default_pid, out, error);
}

static Memmy_Status Memmy_EvalModuleResolver_Push(void *user_data, Memmy_Module *module)
{
    Memmy_EvalModuleResolver *resolver = (Memmy_EvalModuleResolver *)user_data;
    if (!String8_EqNoCase(module->name, resolver->name))
    {
        return Memmy_Status_Ok;
    }

    resolver->match_count++;
    if (resolver->match_count > 1)
    {
        Memmy_EvalError(resolver->error, Memmy_Status_Ambiguous, String8_Lit("target"),
                        String8_Lit("module target is ambiguous"));
        return Memmy_Status_Ambiguous;
    }
    resolver->match = *module;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ResolveModule(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_Module *out,
                                             Memmy_Process **out_process, Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    Memmy_Process *process = 0;
    Memmy_Status status = Memmy_Eval_ResolveTargetProcess(exec, target, &process, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Scratch scratch = Scratch_Begin(&env->arena, 1);
    Memmy_EvalModuleResolver resolver = {
        .name = target->target_module,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_EvalModuleResolver_Push,
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
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("target"), String8_Lit("module target not found"));
        return Memmy_Status_NotFound;
    }

    *out = resolver.match;
    if (out_process != 0)
    {
        *out_process = process;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_Target(Memmy_EvalExec *exec, Memmy_AstNode *target, Memmy_EvalValue *out,
                                      Memmy_Error *error)
{
    if (target->target_module.len != 0)
    {
        Memmy_Module module = {0};
        Memmy_Status status = Memmy_Eval_ResolveModule(exec, target, &module, 0, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Range_FromStartLength(module.base, module.size, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = range};
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("target"), String8_Lit("empty target"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_EvalProcessEmitter_Push(void *user_data, Memmy_ProcessInfo *info)
{
    Memmy_EvalProcessEmitter *emitter = (Memmy_EvalProcessEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(info->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    Memmy_EvalResult_Push(emitter->sink, (Memmy_EvalResult){
                                             .kind = Memmy_EvalResultKind_Process,
                                             .process = *info,
                                         });
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EvalModuleEmitter_Push(void *user_data, Memmy_Module *module)
{
    Memmy_EvalModuleEmitter *emitter = (Memmy_EvalModuleEmitter *)user_data;
    if (emitter->filter.len != 0 && !String8_FuzzyMatchNoCase(module->name, emitter->filter))
    {
        return Memmy_Status_Ok;
    }

    Memmy_EvalResult_Push(emitter->sink, (Memmy_EvalResult){
                                             .kind = Memmy_EvalResultKind_Module,
                                             .module = *module,
                                         });
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_EvalRegionEmitter_Push(void *user_data, Memmy_Region *region)
{
    Memmy_EvalRegionEmitter *emitter = (Memmy_EvalRegionEmitter *)user_data;
    Memmy_EvalResult_Push(emitter->sink, (Memmy_EvalResult){
                                             .kind = Memmy_EvalResultKind_Region,
                                             .region = *region,
                                         });
    return Memmy_Status_Ok;
}

static Memmy_EvalResultKind Memmy_EvalResultKind_ForValue(Memmy_EvalValue value)
{
    if (value.kind == Memmy_EvalValueKind_TypedValue && value.old_typed_value.bytes.data != 0)
    {
        return Memmy_EvalResultKind_Write;
    }
    if (value.kind == Memmy_EvalValueKind_TypedValue)
    {
        return Memmy_EvalResultKind_Read;
    }
    if (value.kind == Memmy_EvalValueKind_AddressList)
    {
        return Memmy_EvalResultKind_AddressList;
    }
    return Memmy_EvalResultKind_Value;
}

static void Memmy_Eval_EmitValueResult(Memmy_EvalResultSink *sink, Memmy_EvalValue value)
{
    Memmy_EvalResult result = {
        .kind = Memmy_EvalResultKind_ForValue(value),
        .value = value,
    };
    if (result.kind == Memmy_EvalResultKind_Write)
    {
        result.address = value.address;
        result.old_value = value.old_typed_value;
        result.new_value = value.typed_value;
    }
    else if (result.kind == Memmy_EvalResultKind_Read)
    {
        result.address = value.address;
        result.new_value = value.typed_value;
    }
    Memmy_EvalResult_Push(sink, result);
}

static Memmy_Status Memmy_Eval_Command(Memmy_EvalExec *exec, Memmy_AstStatement *statement, Memmy_EvalResultSink *sink,
                                       Memmy_Error *error)
{
    Memmy_EvalEnv *env = exec->env;
    if (statement->command_kind == Memmy_AstCommandKind_Procs)
    {
        Memmy_EvalProcessEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ProcessInfoSink process_sink = {
            .callback = Memmy_EvalProcessEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_EnumerateProcesses(env->arena, process_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Mods)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("mods"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalModuleEmitter emitter = {
            .sink = sink,
            .filter = statement->command_arg,
        };
        Memmy_ModuleSink module_sink = {
            .callback = Memmy_EvalModuleEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateModules(env->arena, process, module_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Regions)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("regions"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalRegionEmitter emitter = {.sink = sink};
        Memmy_RegionSink region_sink = {
            .callback = Memmy_EvalRegionEmitter_Push,
            .user_data = &emitter,
        };
        return Memmy_Process_EnumerateRegions(env->arena, process, region_sink, error);
    }
    if (statement->command_kind == Memmy_AstCommandKind_Vars)
    {
        HashMap_ForEach(Memmy_EvalBinding, binding, &env->bindings, hash)
        {
            Memmy_EvalResult_Push(sink, (Memmy_EvalResult){
                                            .kind = Memmy_EvalResultKind_Variable,
                                            .variable =
                                                {
                                                    .name = binding->name,
                                                    .value = binding->value,
                                                },
                                        });
        }
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == Memmy_AstCommandKind_Unset)
    {
        Memmy_Status status = Memmy_EvalEnv_Unset(env, statement->command_arg);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_EvalResult_Push(sink, (Memmy_EvalResult){
                                        .kind = Memmy_EvalResultKind_Unset,
                                        .name = statement->command_arg,
                                    });
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == Memmy_AstCommandKind_Clear)
    {
        Memmy_EvalEnv_Clear(env);
        Memmy_EvalResult_Push(sink, (Memmy_EvalResult){.kind = Memmy_EvalResultKind_Clear});
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == Memmy_AstCommandKind_Help)
    {
        Memmy_EvalResult_Push(sink,
                              (Memmy_EvalResult){
                                  .kind = Memmy_EvalResultKind_Help,
                                  .text = String8_Lit("Core values:\n"
                                                      "  x                    constant integer/math expression\n"
                                                      "  @x                   absolute address\n"
                                                      "  [@a..@b]             explicit address range [a, b)\n"
                                                      "  [@a..+n]             sized address range [a, a+n)\n"
                                                      "  <module>             module range in selected process\n"
                                                      "  [0..]                selected process readable regions\n"
                                                      "  function address     function range containing address\n"
                                                      "  $name                variable\n"
                                                      "\n"
                                                      "Memory:\n"
                                                      "  range refs <ptr|rel32|any> address\n"
                                                      "  list => expr         transform each address/range item\n"
                                                      "  $                    current item inside transform RHS\n"
                                                      "  $matches => [$..+0x20]\n"
                                                      "  $xrefs => function $\n"
                                                      "  $ranges => $ + 4\n"
                                                      "  $name[N]             index address/range list\n"
                                                      "\n"
                                                      "Commands:\n"
                                                      "  /procs [filter]\n"
                                                      "  /attach <pid|name>  select process and clear variables\n"
                                                      "  /detach             clear selected process and variables\n"
                                                      "  /mods [filter]\n"
                                                      "  /regions\n"
                                                      "  /vars\n"
                                                      "  /unset $var\n"
                                                      "  /clear\n"
                                                      "  /help\n"
                                                      "  /exit\n"
                                                      "  /quit\n"),
                              });
        return Memmy_Status_Ok;
    }
    if (statement->command_kind == Memmy_AstCommandKind_Exit || statement->command_kind == Memmy_AstCommandKind_Quit)
    {
        Memmy_EvalResult_Push(sink, (Memmy_EvalResult){.kind = Memmy_EvalResultKind_Exit});
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("command"), String8_Lit("unknown command"));
    return Memmy_Status_InvalidArgument;
}

static Memmy_Status Memmy_EvalTransform_Append(Memmy_EvalExec *exec, Memmy_EvalValue value, List *addresses,
                                               List *ranges, Memmy_EvalValueKind *out_kind, Memmy_Error *error)
{
    Memmy_EvalValueKind value_kind = Memmy_EvalValueKind_Null;
    Memmy_Status status = Memmy_EvalTransform_ListKindForValue(value, &value_kind, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    if (*out_kind == Memmy_EvalValueKind_Null)
    {
        *out_kind = value_kind;
    }
    else if (*out_kind != value_kind)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                        String8_Lit("transform expression produced mixed address and range values"));
        return Memmy_Status_InvalidArgument;
    }

    Arena *arena = exec->env->arena;
    if (value.kind == Memmy_EvalValueKind_Address)
    {
        Memmy_EvalAddressList_Push(arena, addresses, value.address);
    }
    else if (value.kind == Memmy_EvalValueKind_AddressList)
    {
        for (U64 i = 0; i < value.address_count; i++)
        {
            Memmy_EvalAddressList_Push(arena, addresses, value.addresses[i]);
        }
    }
    else if (value.kind == Memmy_EvalValueKind_Range)
    {
        Memmy_EvalRangeList_Push(arena, ranges, value.range);
    }
    else if (value.kind == Memmy_EvalValueKind_RangeList)
    {
        for (U64 i = 0; i < value.range_count; i++)
        {
            Memmy_EvalRangeList_Push(arena, ranges, value.ranges[i]);
        }
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ListTransform(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out,
                                             Memmy_Error *error)
{
    Memmy_EvalValue list = {0};
    Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &list, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (list.kind != Memmy_EvalValueKind_AddressList && list.kind != Memmy_EvalValueKind_RangeList)
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                        String8_Lit("expected address list or range list"));
        return Memmy_Status_InvalidArgument;
    }

    U64 count = list.kind == Memmy_EvalValueKind_AddressList ? list.address_count : list.range_count;
    if (count == 0)
    {
        Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("transform"),
                        String8_Lit("transform input list is empty"));
        return Memmy_Status_NotFound;
    }

    List addresses = {0}; // Memmy_EvalAddressNode
    List ranges = {0};    // Memmy_EvalRangeNode
    Memmy_EvalValueKind out_kind = Memmy_EvalValueKind_Null;
    B32 old_has_current_item = exec->has_current_item;
    Memmy_EvalValue old_current_item = exec->current_item;

    for (U64 i = 0; i < count; i++)
    {
        exec->has_current_item = 1;
        if (list.kind == Memmy_EvalValueKind_AddressList)
        {
            exec->current_item = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = list.addresses[i]};
        }
        else
        {
            exec->current_item = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = list.ranges[i]};
        }

        Memmy_EvalValue item_result = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &item_result, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
        status = Memmy_EvalTransform_Append(exec, item_result, &addresses, &ranges, &out_kind, error);
        if (status != Memmy_Status_Ok)
        {
            exec->has_current_item = old_has_current_item;
            exec->current_item = old_current_item;
            return status;
        }
    }

    exec->has_current_item = old_has_current_item;
    exec->current_item = old_current_item;
    if (out_kind == Memmy_EvalValueKind_RangeList)
    {
        *out = Memmy_Eval_RangeListFromList(exec->env->arena, &ranges);
    }
    else
    {
        *out = Memmy_Eval_AddressListFromList(exec->env->arena, &addresses);
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Eval_ReadPointer(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out,
                                           Memmy_Error *error)
{
    if (process == 0 || !Memmy_Process_IsOpen(process))
    {
        Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
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
        Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("address"),
                        String8_Lit("target pointer width is unknown"));
        return Memmy_Status_Unsupported;
    }

    U8 bytes[8] = {0};
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Process_Read(process, address, bytes, pointer_size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != pointer_size)
    {
        Memmy_EvalError(error, Memmy_Status_PartialRead, String8_Lit("address"),
                        String8_Lit("pointer read returned too few bytes"));
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

Memmy_EvalEnv *Memmy_EvalEnv_Create(Arena *arena)
{
    if (arena == 0)
    {
        return 0;
    }

    Memmy_EvalEnv *env = Arena_PushStruct(arena, Memmy_EvalEnv);
    env->arena = arena;
    env->bindings = HashMap_Create(arena);
    return env;
}

void Memmy_EvalEnv_SetDefaultProcess(Memmy_EvalEnv *env, U32 pid, Memmy_PointerWidth pointer_width)
{
    if (env != 0)
    {
        env->has_default_process = 1;
        env->default_pid = pid;
        env->default_pointer_width = pointer_width;
    }
}

void Memmy_EvalEnv_ClearDefaultProcess(Memmy_EvalEnv *env)
{
    if (env != 0)
    {
        env->has_default_process = 0;
        env->default_pid = 0;
        env->default_pointer_width = Memmy_PointerWidth_Unknown;
    }
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

static Memmy_Status Memmy_EvalStatementWithContext(Memmy_EvalExec *exec, Memmy_AstStatement *statement,
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

static Memmy_Status Memmy_EvalExprWithContext(Memmy_EvalExec *exec, Memmy_AstNode *expr, Memmy_EvalValue *out,
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

    if (expr->kind == Memmy_AstNodeKind_ConstArithmetic)
    {
        if (expr->op == Memmy_AstConstOp_None)
        {
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = expr->value};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue lhs = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &lhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->op == Memmy_AstConstOp_Pos || expr->op == Memmy_AstConstOp_Neg)
        {
            I64 constant = 0;
            status = Memmy_EvalValue_AsConst(&lhs, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (expr->op == Memmy_AstConstOp_Neg && !SubI64Checked(0, constant, &constant))
            {
                Memmy_EvalError(error, Memmy_Status_Overflow, String8_Lit("expr"),
                                String8_Lit("constant arithmetic overflow"));
                return Memmy_Status_Overflow;
            }
            *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Const, .constant = constant};
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue rhs = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        return Memmy_Eval_ApplyBinary(expr->op, lhs, rhs, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_Variable)
    {
        return Memmy_EvalEnv_Find(env, expr->name, out);
    }
    if (expr->kind == Memmy_AstNodeKind_CurrentItem)
    {
        if (!exec->has_current_item)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("transform"),
                            String8_Lit("current item is only available inside transforms"));
            return Memmy_Status_InvalidArgument;
        }
        *out = exec->current_item;
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ListTransform)
    {
        return Memmy_Eval_ListTransform(exec, expr, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_Target)
    {
        return Memmy_Eval_Target(exec, expr, out, error);
    }
    if (expr->kind == Memmy_AstNodeKind_ProcessRange)
    {
        Memmy_Process *process = 0;
        Memmy_Status status = Memmy_Eval_RequireProcess(exec, 0, String8_Lit("range"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        (void)process;
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_ProcessRange};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Address)
    {
        if (expr->value_expr != 0)
        {
            Memmy_EvalValue value = {0};
            Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->value_expr, &value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            I64 constant = 0;
            status = Memmy_EvalValue_AsConst(&value, &constant, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (constant < 0)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("address"),
                                String8_Lit("address cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = (Memmy_Addr)constant,
            };
            return Memmy_Status_Ok;
        }

        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalValue offset_value = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &offset_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 offset = 0;
        status = Memmy_EvalValue_AsConst(&offset_value, &offset, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Eval_AddressAddConst(address, offset, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_Address,
            .address = address,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Range)
    {
        Memmy_EvalValue start_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &start_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr start = 0;
        status = Memmy_EvalValue_AsAddress(&start_value, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalValue rhs = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &rhs, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        if (expr->range_is_sized)
        {
            I64 size = 0;
            status = Memmy_EvalValue_AsConst(&rhs, &size, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            if (size < 0)
            {
                Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("range"),
                                String8_Lit("range size cannot be negative"));
                return Memmy_Status_InvalidArgument;
            }
            status = Memmy_Range_FromStartLength(start, (Memmy_Size)size, &range, error);
        }
        else
        {
            Memmy_Addr end = 0;
            status = Memmy_EvalValue_AsAddress(&rhs, &end, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Range_FromStartEnd(start, end, &range, error);
        }
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_Range,
            .range = range,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Function)
    {
        Memmy_EvalValue value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&value, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &value, String8_Lit("function"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Range range = {0};
        status = Memmy_Process_FindFunction(env->arena, process, address, &range, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Range, .range = range};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Deref)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("address"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        status = Memmy_Eval_ReadPointer(process, address, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (expr->rhs != 0)
        {
            Memmy_EvalValue offset_value = {0};
            status = Memmy_EvalExprWithContext(exec, expr->rhs, &offset_value, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            I64 offset = 0;
            status = Memmy_EvalValue_AsConst(&offset_value, &offset, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
            status = Memmy_Eval_AddressAddConst(address, offset, &address, error);
            if (status != Memmy_Status_Ok)
            {
                return status;
            }
        }
        *out = (Memmy_EvalValue){.kind = Memmy_EvalValueKind_Address, .address = address};
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_TypedRead)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("read"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value value = {0};
        status = Memmy_Eval_ReadValue(env->arena, process, address, type, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_TypedValue,
            .constant = Memmy_Eval_IntegerFromBytes(value),
            .address = address,
            .typed_value = value,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_TypedWrite)
    {
        Memmy_EvalValue base = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &base, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &base, String8_Lit("write"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr address = 0;
        status = Memmy_EvalValue_AsAddress(&base, &address, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value old_value = {0};
        status = Memmy_Eval_ReadValue(env->arena, process, address, type, &old_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value new_value = {0};
        status = Memmy_Eval_ParseValue(exec, process, type, expr->value_text, &new_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        U64 bytes_written = 0;
        status =
            Memmy_Process_Write(process, address, new_value.bytes.data, new_value.bytes.len, &bytes_written, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (bytes_written != new_value.bytes.len)
        {
            Memmy_EvalError(error, Memmy_Status_PartialWrite, String8_Lit("write"), String8_Lit("partial typed write"));
            if (error != 0)
            {
                error->byte_count = bytes_written;
            }
            return Memmy_Status_PartialWrite;
        }

        *out = (Memmy_EvalValue){
            .kind = Memmy_EvalValueKind_TypedValue,
            .constant = Memmy_Eval_IntegerFromBytes(new_value),
            .address = address,
            .typed_value = new_value,
            .old_typed_value = old_value,
        };
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_PatternScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Pattern pattern = {0};
        status = Memmy_Pattern_Parse(env->arena, expr->pattern, Memmy_PatternParseFlag_AllowWildcards, &pattern, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanPattern(env->arena, process, &options, pattern, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ValueScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_Type type = {0};
        status = Memmy_Eval_ParseType(expr->type_name, &type, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Value value = {0};
        status = Memmy_Eval_ParseValue(exec, process, type, expr->value_text, &value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanValue(env->arena, process, &options, value, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_ReferenceScan)
    {
        Memmy_EvalValue range_value = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &range_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Process *process = 0;
        status = Memmy_Eval_RequireProcess(exec, &range_value, String8_Lit("scan"), &process, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (range_value.kind != Memmy_EvalValueKind_Range && range_value.kind != Memmy_EvalValueKind_ProcessRange)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                            String8_Lit("expected scan range"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_EvalValue target_value = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &target_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr target = 0;
        status = Memmy_EvalValue_AsAddress(&target_value, &target, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_EvalScanCollector collector = {.arena = env->arena};
        Memmy_ScanSink sink = {
            .callback = Memmy_EvalScanCollector_Push,
            .user_data = &collector,
        };
        Memmy_ScanOptions options = {
            .range = range_value.range,
            .limit = 0,
            .chunk_size = 0,
            .scan_readable_regions = range_value.kind == Memmy_EvalValueKind_ProcessRange,
        };
        status = Memmy_Process_ScanReferences(env->arena, process, &options,
                                              Memmy_Eval_ReferenceScanMode(expr->reference_mode), target, sink, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        *out = Memmy_Eval_AddressListFromCollector(env->arena, &collector);
        return Memmy_Status_Ok;
    }
    if (expr->kind == Memmy_AstNodeKind_Index)
    {
        Memmy_EvalValue list = {0};
        Memmy_Status status = Memmy_EvalExprWithContext(exec, expr->lhs, &list, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (list.kind != Memmy_EvalValueKind_AddressList && list.kind != Memmy_EvalValueKind_RangeList)
        {
            Memmy_EvalError(error, Memmy_Status_InvalidArgument, String8_Lit("index"),
                            String8_Lit("expected address list or range list"));
            return Memmy_Status_InvalidArgument;
        }

        Memmy_EvalValue index_value = {0};
        status = Memmy_EvalExprWithContext(exec, expr->rhs, &index_value, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        I64 index = 0;
        status = Memmy_EvalValue_AsConst(&index_value, &index, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        U64 count = list.kind == Memmy_EvalValueKind_AddressList ? list.address_count : list.range_count;
        if (index < 0 || (U64)index >= count)
        {
            Memmy_EvalError(error, Memmy_Status_NotFound, String8_Lit("index"), String8_Lit("list index out of range"));
            return Memmy_Status_NotFound;
        }

        if (list.kind == Memmy_EvalValueKind_AddressList)
        {
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Address,
                .address = list.addresses[index],
            };
        }
        else
        {
            *out = (Memmy_EvalValue){
                .kind = Memmy_EvalValueKind_Range,
                .range = list.ranges[index],
            };
        }
        return Memmy_Status_Ok;
    }

    Memmy_EvalError(error, Memmy_Status_Unsupported, String8_Lit("expr"),
                    String8_Lit("expression kind is not implemented yet"));
    return Memmy_Status_Unsupported;
}

Memmy_Status Memmy_EvalEnv_Set(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue value)
{
    if (env == 0 || name.len == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        binding = Arena_PushStruct(env->arena, Memmy_EvalBinding);
        binding->name = String8_Copy(env->arena, name);
        binding->hash.hash = Hash_Fnv1a(binding->name.data, binding->name.len);
        HashMap_Insert(&env->bindings, &binding->hash);
    }
    binding->value = Memmy_EvalValue_Copy(env->arena, value);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_EvalEnv_Find(Memmy_EvalEnv *env, String8 name, Memmy_EvalValue *out)
{
    if (out != 0)
    {
        *out = (Memmy_EvalValue){0};
    }
    if (env == 0 || out == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }

    *out = binding->value;
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_EvalEnv_Unset(Memmy_EvalEnv *env, String8 name)
{
    if (env == 0)
    {
        return Memmy_Status_InvalidArgument;
    }

    Memmy_EvalBinding *binding = Memmy_EvalEnv_FindBinding(env, name);
    if (binding == 0)
    {
        return Memmy_Status_NotFound;
    }
    HashMap_Remove(&env->bindings, &binding->hash);
    return Memmy_Status_Ok;
}

void Memmy_EvalEnv_Clear(Memmy_EvalEnv *env)
{
    if (env != 0)
    {
        env->bindings = HashMap_Create(env->arena);
    }
}
