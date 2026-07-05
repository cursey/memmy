#include "memmy_scan.h"

#include "base_checked.h"
#include "base_memory.h"
#include "base_sort.h"
#include "memmy_backend.h"

static B32 Memmy_Pattern_MatchesAt(Memmy_Pattern pattern, U8 *bytes)
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

typedef B32 Memmy_ScanMatchFn(void *user_data, U8 *bytes);

typedef struct Memmy_ScanNeedle Memmy_ScanNeedle;
struct Memmy_ScanNeedle
{
    U64 size;
    Memmy_ScanMatchFn *match;
    void *user_data;
};

static B32 Memmy_Value_MatchesAt(void *user_data, U8 *bytes)
{
    Memmy_Value *value = (Memmy_Value *)user_data;
    return Memory_Equals(value->bytes.data, bytes, value->bytes.len);
}

static B32 Memmy_Pattern_MatchesAtUserData(void *user_data, U8 *bytes)
{
    Memmy_Pattern *pattern = (Memmy_Pattern *)user_data;
    return Memmy_Pattern_MatchesAt(*pattern, bytes);
}

static B32 Memmy_Scan_IsReadableRegion(Memmy_Region *region)
{
    return region->state == Memmy_RegionState_Committed && (region->access & Memmy_RegionAccess_Read) != 0 &&
           (region->access & Memmy_RegionAccess_Guard) == 0;
}

static I32 Memmy_ScanRange_Cmp(void *a, void *b, void *ctx)
{
    Unused(ctx);

    Memmy_Range *range_a = (Memmy_Range *)a;
    Memmy_Range *range_b = (Memmy_Range *)b;
    if (range_a->start < range_b->start)
    {
        return -1;
    }
    if (range_a->start > range_b->start)
    {
        return 1;
    }
    if (range_a->end < range_b->end)
    {
        return -1;
    }
    if (range_a->end > range_b->end)
    {
        return 1;
    }
    return 0;
}

static Memmy_Status Memmy_Scan_EmitMatch(Memmy_ScanSink sink, Memmy_Addr address, Memmy_ScanOptions *options,
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

static Memmy_Status Memmy_Process_ScanRange(Arena *arena, Memmy_Process *process, Memmy_Range range,
                                            Memmy_ScanOptions *options, Memmy_ScanNeedle needle, Memmy_ScanSink sink,
                                            U64 *match_count, B32 *any_read, B32 *out_stop, Memmy_Addr *last_match,
                                            B32 *has_last_match, Memmy_Error *error)
{
    U64 chunk_size = options->chunk_size != 0 ? options->chunk_size : MEMMY_DEFAULT_SCAN_CHUNK_SIZE;
    chunk_size = Max(chunk_size, needle.size);
    U64 overlap = needle.size - 1;
    U64 step = chunk_size - overlap;
    if (step == 0)
    {
        step = 1;
    }

    U8 *buffer = Arena_PushArrayNoZero(arena, U8, chunk_size);
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
                if (bytes_read >= needle.size)
                {
                    for (U64 i = 0; i <= bytes_read - needle.size; i++)
                    {
                        Memmy_Addr match_addr = pos + i;
                        if (match_addr < range.start || match_addr + needle.size > range.end)
                        {
                            continue;
                        }
                        if (needle.match(needle.user_data, buffer + i))
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

static Memmy_Status Memmy_Process_ScanNeedle(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
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
    if (needle.size == 0 || needle.match == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"), String8_Lit("scan needle is empty"));
        return Memmy_Status_InvalidArgument;
    }

    B32 any_read = 0;
    B32 stop = 0;
    U64 match_count = 0;
    Memmy_Addr last_match = 0;
    B32 has_last_match = 0;
    Memmy_Backend *backend = process->backend;
    if (backend != 0 && backend->list_regions != 0)
    {
        Scratch scratch = Scratch_Begin(&arena, 1);
        Memmy_RegionList regions = {0};
        Memmy_Status status = Memmy_Process_ListRegions(scratch.arena, process, &regions, error);
        if (status == Memmy_Status_Unsupported)
        {
            Scratch_End(scratch);
        }
        else if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        else
        {
            Memmy_Range *scan_ranges = Arena_PushArrayNoZero(scratch.arena, Memmy_Range, regions.list.count);
            U64 scan_range_count = 0;
            List_ForEach(Memmy_Region, region, &regions.list, link)
            {
                if (!Memmy_Scan_IsReadableRegion(region))
                {
                    continue;
                }

                Memmy_Addr region_end = 0;
                if (!AddU64Checked(region->base, region->size, &region_end))
                {
                    continue;
                }

                Memmy_Range scan_range = {
                    .start = Max(options->range.start, region->base),
                    .end = Min(options->range.end, region_end),
                };
                if (scan_range.end <= scan_range.start)
                {
                    continue;
                }

                scan_ranges[scan_range_count++] = scan_range;
            }

            Sort(scan_ranges, scan_range_count, sizeof(scan_ranges[0]), Memmy_ScanRange_Cmp, 0);
            for (U64 i = 0; i < scan_range_count && !stop;)
            {
                Memmy_Range merged_range = scan_ranges[i++];
                while (i < scan_range_count && scan_ranges[i].start <= merged_range.end)
                {
                    merged_range.end = Max(merged_range.end, scan_ranges[i].end);
                    i++;
                }

                status = Memmy_Process_ScanRange(arena, process, merged_range, options, needle, sink, &match_count,
                                                 &any_read, &stop, &last_match, &has_last_match, error);
                if (status != Memmy_Status_Ok || stop)
                {
                    Scratch_End(scratch);
                    return status;
                }
            }

            Scratch_End(scratch);
            if (!any_read)
            {
                Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("scan"),
                                String8_Lit("scan range is unreadable"));
                return Memmy_Status_Unreadable;
            }
            return Memmy_Status_Ok;
        }
    }

    Memmy_Status status = Memmy_Process_ScanRange(arena, process, options->range, options, needle, sink, &match_count,
                                                  &any_read, &stop, &last_match, &has_last_match, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!any_read)
    {
        Memmy_Error_Set(error, Memmy_Status_Unreadable, String8_Lit("scan"), String8_Lit("scan range is unreadable"));
        return Memmy_Status_Unreadable;
    }
    return Memmy_Status_Ok;
}

Memmy_Status Memmy_Process_ScanValue(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                     Memmy_Value value, Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanNeedle needle = {
        .size = value.bytes.len,
        .match = value.bytes.data != 0 ? Memmy_Value_MatchesAt : 0,
        .user_data = &value,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}

Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern, Memmy_ScanSink sink, Memmy_Error *error)
{
    Memmy_ScanNeedle needle = {
        .size = pattern.count,
        .match = pattern.bytes != 0 ? Memmy_Pattern_MatchesAtUserData : 0,
        .user_data = &pattern,
    };
    return Memmy_Process_ScanNeedle(arena, process, options, needle, sink, error);
}
