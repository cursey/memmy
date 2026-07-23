#include "memmy_scan.h"

#include "base.h"

static B32 Memmy_Pattern_MatchesAt(Memmy_Pattern pattern, U8 const *bytes)
{
    for (U64 i = 0; i < pattern.count; i++)
    {
        if (!pattern.bytes[i].wildcard && pattern.bytes[i].value != bytes[i])
        {
            return 0;
        }
    }
    return 1;
}

typedef struct Memmy_ScanNeedle Memmy_ScanNeedle;
struct Memmy_ScanNeedle
{
    U64 min_size;
    U64 max_size;
    Memmy_ScanMatchFn *match;
    void *user_data;
};

static B32 Memmy_EncodedValue_MatchesAt(void *user_data, Memmy_Addr address, U8 const *bytes, U64 available)
{
    Unused(address);

    Memmy_EncodedValue *value = (Memmy_EncodedValue *)user_data;
    return available >= value->bytes.len && Memory_Equals(value->bytes.data, bytes, value->bytes.len);
}

static B32 Memmy_Pattern_MatchesAtUserData(void *user_data, Memmy_Addr address, U8 const *bytes, U64 available)
{
    Unused(address);

    Memmy_Pattern *pattern = (Memmy_Pattern *)user_data;
    return available >= pattern->count && Memmy_Pattern_MatchesAt(*pattern, bytes);
}

typedef struct Memmy_ReferenceScanNeedle Memmy_ReferenceScanNeedle;
struct Memmy_ReferenceScanNeedle
{
    Memmy_ReferenceScanMode mode;
    Memmy_Addr target;
    U64 ptr_size;
};

static U64 Memmy_Integer_ReadLE(U8 const *bytes, U64 size)
{
    U64 result = 0;
    for (U64 i = 0; i < size; i++)
    {
        result |= ((U64)bytes[i]) << (i * 8);
    }
    return result;
}

static B32 Memmy_ReferenceScan_PtrMatches(Memmy_ReferenceScanNeedle *needle, U8 const *bytes, U64 available)
{
    if (available < needle->ptr_size)
    {
        return 0;
    }
    return Memmy_Integer_ReadLE(bytes, needle->ptr_size) == needle->target;
}

static B32 Memmy_ReferenceScan_Rel32Matches(Memmy_ReferenceScanNeedle *needle, Memmy_Addr address, U8 const *bytes,
                                            U64 available)
{
    if (available < 4)
    {
        return 0;
    }

    U32 raw = (U32)Memmy_Integer_ReadLE(bytes, 4);
    I64 disp = (I64)(I32)raw;
    Memmy_Addr base = 0;
    if (!AddU64Checked(address, 4, &base))
    {
        return 0;
    }

    Memmy_Addr ref_target = 0;
    if (disp < 0)
    {
        U64 magnitude = (U64)(-(disp + 1)) + 1;
        if (base < magnitude)
        {
            return 0;
        }
        ref_target = base - magnitude;
    }
    else if (!AddU64Checked(base, (U64)disp, &ref_target))
    {
        return 0;
    }

    return ref_target == needle->target;
}

static B32 Memmy_ReferenceScan_MatchesAt(void *user_data, Memmy_Addr address, U8 const *bytes, U64 available)
{
    Memmy_ReferenceScanNeedle *needle = (Memmy_ReferenceScanNeedle *)user_data;
    if (needle->mode == Memmy_ReferenceScanMode_Ptr)
    {
        return Memmy_ReferenceScan_PtrMatches(needle, bytes, available);
    }
    if (needle->mode == Memmy_ReferenceScanMode_Rel32)
    {
        return Memmy_ReferenceScan_Rel32Matches(needle, address, bytes, available);
    }
    if (needle->mode == Memmy_ReferenceScanMode_Any)
    {
        return Memmy_ReferenceScan_PtrMatches(needle, bytes, available) ||
               Memmy_ReferenceScan_Rel32Matches(needle, address, bytes, available);
    }
    return 0;
}

static Memmy_Status Memmy_Scan_EmitMatch(Memmy_ScanSink sink, Memmy_Addr address, Memmy_ScanOptions const *options,
                                         U64 *match_count, B32 *out_stop)
{
    if (options->limit != 0 && *match_count >= options->limit)
    {
        *out_stop = 1;
        return Memmy_Status_Ok;
    }

    Memmy_Status status = sink.callback(sink.user_data, address);
    if (status != Memmy_Status_Ok)
    {
        *out_stop = 1;
        return status;
    }

    *match_count += 1;
    if (options->limit != 0 && *match_count >= options->limit)
    {
        *out_stop = 1;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Process_ScanRange(U8 *buffer, Memmy_Process *process, Memmy_Range range,
                                            Memmy_ScanOptions const *options, Memmy_ScanNeedle needle,
                                            Memmy_ScanSink sink, U64 *match_count, B32 *any_read, B32 *out_stop,
                                            Memmy_Addr *last_match, B32 *has_last_match, Memmy_Error *error)
{
    U64 chunk_size = options->chunk_size != 0 ? options->chunk_size : MEMMY_DEFAULT_SCAN_CHUNK_SIZE;
    chunk_size = Max(chunk_size, needle.max_size);
    U64 overlap = needle.max_size - 1;
    U64 step = chunk_size - overlap;
    if (step == 0)
    {
        step = 1;
    }

    Memmy_Addr pos = range.start;
    while (pos < range.end && !*out_stop)
    {
        U64 remaining = range.end - pos;
        U64 to_read = Min(chunk_size, remaining);
        U64 bytes_read = 0;
        U64 advance = step;
        Memmy_Status status = Memmy_Process_Read(process, pos, buffer, to_read, &bytes_read, error);

        if (status == Memmy_Status_Ok || status == Memmy_Status_PartialRead)
        {
            if (bytes_read > 0)
            {
                *any_read = 1;
                if (bytes_read >= needle.min_size)
                {
                    for (U64 i = 0; i <= bytes_read - needle.min_size; i++)
                    {
                        Memmy_Addr match_addr = pos + i;
                        if (match_addr < range.start || match_addr + needle.min_size > range.end)
                        {
                            continue;
                        }
                        U64 available = Min(bytes_read - i, range.end - match_addr);
                        if (needle.match(needle.user_data, match_addr, buffer + i, available))
                        {
                            if (*has_last_match && match_addr <= *last_match)
                            {
                                continue;
                            }
                            Memmy_Status sink_status =
                                Memmy_Scan_EmitMatch(sink, match_addr, options, match_count, out_stop);
                            *last_match = match_addr;
                            *has_last_match = 1;
                            if (sink_status != Memmy_Status_Ok || *out_stop)
                            {
                                return sink_status;
                            }
                        }
                    }
                }
            }
            if (status == Memmy_Status_PartialRead)
            {
                advance = bytes_read > overlap ? bytes_read - overlap : 1;
            }
        }
        else if (status == Memmy_Status_Unreadable)
        {
            if (bytes_read > 0)
            {
                *any_read = 1;
                advance = bytes_read > overlap ? bytes_read - overlap : 1;
            }
            else
            {
                advance = 1;
            }
        }
        else
        {
            return status;
        }

        if (remaining <= advance)
        {
            break;
        }
        pos += advance;
    }

    return Memmy_Status_Ok;
}

typedef struct Memmy_ScanRangeTraversal Memmy_ScanRangeTraversal;
struct Memmy_ScanRangeTraversal
{
    U8 *buffer;
    Memmy_Process *process;
    Memmy_ScanOptions const *options;
    Memmy_ScanNeedle needle;
    Memmy_ScanSink sink;
    U64 match_count;
    B32 any_read;
    B32 stop;
    Memmy_Addr last_match;
    B32 has_last_match;
    Memmy_Error *error;
};

static Memmy_Status Memmy_ScanRangeTraversal_Scan(void *user_data, Memmy_Range range)
{
    Memmy_ScanRangeTraversal *traversal = (Memmy_ScanRangeTraversal *)user_data;
    if (traversal->stop)
    {
        return Memmy_Status_Ok;
    }

    return Memmy_Process_ScanRange(traversal->buffer, traversal->process, range, traversal->options, traversal->needle,
                                   traversal->sink, &traversal->match_count, &traversal->any_read, &traversal->stop,
                                   &traversal->last_match, &traversal->has_last_match, traversal->error);
}

static Memmy_Status Memmy_Process_ScanNeedle(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                             Memmy_ScanNeedle needle, Memmy_ScanSink sink, Memmy_Error *error)
{
    if (arena == 0 || process == 0 || options == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"), String8_Lit("missing scan argument"));
        return Memmy_Status_InvalidArgument;
    }
    if (options->range.end < options->range.start)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                        String8_Lit("scan range end is before start"));
        return Memmy_Status_InvalidArgument;
    }

    if (options->range.end == options->range.start)
    {
        return Memmy_Status_Ok;
    }
    if (needle.min_size == 0 || needle.max_size < needle.min_size || needle.match == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"), String8_Lit("scan needle is empty"));
        return Memmy_Status_InvalidArgument;
    }

    U64 chunk_size = options->chunk_size != 0 ? options->chunk_size : MEMMY_DEFAULT_SCAN_CHUNK_SIZE;
    chunk_size = Max(chunk_size, needle.max_size);
    Scratch scratch = Scratch_Begin(&arena, 1);
    U8 *buffer = Arena_PushArrayNoZero(scratch.arena, U8, chunk_size);
    Memmy_ScanRangeTraversal traversal = {
        .buffer = buffer,
        .process = process,
        .options = options,
        .needle = needle,
        .sink = sink,
        .error = error,
    };
    Memmy_RangeSink range_sink = {
        .callback = Memmy_ScanRangeTraversal_Scan,
        .user_data = &traversal,
    };
    Memmy_Status status = Memmy_Process_EnumerateAccessibleRanges(arena, process, options->range,
                                                                  Memmy_RegionAccess_Read, range_sink, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }
    if (!traversal.any_read)
    {
        Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("scan"), String8_Lit("scan range is unreadable"));
        Scratch_End(scratch);
        return Memmy_Status_Unreadable;
    }
    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Process_ScanValue(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                     Memmy_EncodedValue value, Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanNeedle needle = {
        .min_size = value.bytes.len,
        .max_size = value.bytes.len,
        .match = value.bytes.data != 0 ? Memmy_EncodedValue_MatchesAt : 0,
        .user_data = &value,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}

Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                       Memmy_Pattern pattern, Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanNeedle needle = {
        .min_size = pattern.count,
        .max_size = pattern.count,
        .match = pattern.bytes != 0 ? Memmy_Pattern_MatchesAtUserData : 0,
        .user_data = &pattern,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}

Memmy_Status Memmy_Process_ScanReferences(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                          Memmy_ReferenceScanMode mode, Memmy_Addr target, Memmy_ScanSink sink,
                                          Memmy_Error *error)
{
    if (arena == 0 || process == 0 || options == 0 || sink.callback == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"), String8_Lit("missing scan argument"));
        return Memmy_Status_InvalidArgument;
    }
    if (mode != Memmy_ReferenceScanMode_Ptr && mode != Memmy_ReferenceScanMode_Rel32 &&
        mode != Memmy_ReferenceScanMode_Any)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"),
                        String8_Lit("invalid reference scan mode"));
        return Memmy_Status_InvalidArgument;
    }

    U64 ptr_size = 0;
    if (mode == Memmy_ReferenceScanMode_Ptr || mode == Memmy_ReferenceScanMode_Any)
    {
        if (process->pointer_width == Memmy_PointerWidth_Unknown)
        {
            Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("scan"),
                            String8_Lit("target pointer width is unknown"));
            return Memmy_Status_Unsupported;
        }
        ptr_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    }

    Memmy_ReferenceScanNeedle reference = {
        .mode = mode,
        .target = target,
        .ptr_size = ptr_size,
    };
    Memmy_ScanNeedle needle = {
        .min_size = mode == Memmy_ReferenceScanMode_Ptr ? ptr_size : 4,
        .max_size = mode == Memmy_ReferenceScanMode_Rel32 ? 4 : ptr_size,
        .match = Memmy_ReferenceScan_MatchesAt,
        .user_data = &reference,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}

Memmy_Status Memmy_Process_ScanCustom(Arena *arena, Memmy_Process *process, Memmy_ScanOptions const *options,
                                      U64 min_size, U64 max_size, Memmy_ScanMatchFn *match, void *user_data,
                                      Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanNeedle needle = {
        .min_size = min_size,
        .max_size = max_size,
        .match = match,
        .user_data = user_data,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}
