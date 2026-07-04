#include "memmy_scan.h"

#include "base_checked.h"
#include "memmy_backend.h"

#include <string.h>

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

static B32 Memmy_Scan_IsReadableRegion(Memmy_Region *region)
{
    return region->state == Memmy_RegionState_Committed && (region->access & Memmy_RegionAccess_Read) != 0 &&
           (region->access & Memmy_RegionAccess_Guard) == 0;
}

static Memmy_Status Memmy_Scan_PushMatch(Arena *arena, Memmy_ScanResultList *out, Memmy_Addr address,
                                         Memmy_ScanOptions *options, B32 *out_stop)
{
    if (options->limit != 0 && out->list.count >= options->limit)
    {
        *out_stop = 1;
        return Memmy_Status_Ok;
    }

    Memmy_ScanResult *result = Memmy_ScanResultList_Push(arena, out);
    result->address = address;

    if (options->limit != 0 && out->list.count >= options->limit)
    {
        *out_stop = 1;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Process_ScanPatternRange(Arena *arena, Memmy_Process *process, Memmy_Range range,
                                                   Memmy_ScanOptions *options, Memmy_Pattern pattern,
                                                   Memmy_ScanResultList *out, B32 *any_read, B32 *out_stop,
                                                   Memmy_Addr *last_match, B32 *has_last_match, Memmy_Error *error)
{
    U64 chunk_size = options->chunk_size != 0 ? options->chunk_size : MEMMY_DEFAULT_SCAN_CHUNK_SIZE;
    chunk_size = Max(chunk_size, pattern.count);
    U64 overlap = pattern.count - 1;
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
                if (bytes_read >= pattern.count)
                {
                    for (U64 i = 0; i <= bytes_read - pattern.count; i++)
                    {
                        Memmy_Addr match_addr = pos + i;
                        if (match_addr < range.start || match_addr + pattern.count > range.end)
                        {
                            continue;
                        }
                        if (Memmy_Pattern_MatchesAt(pattern, buffer + i))
                        {
                            if (*has_last_match && match_addr <= *last_match)
                            {
                                continue;
                            }
                            Memmy_Status push_status = Memmy_Scan_PushMatch(arena, out, match_addr, options, out_stop);
                            *last_match = match_addr;
                            *has_last_match = 1;
                            if (push_status != Memmy_Status_Ok || *out_stop)
                            {
                                return push_status;
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

Memmy_ScanResult *Memmy_ScanResultList_Push(Arena *arena, Memmy_ScanResultList *list)
{
    Memmy_ScanResult *result = Arena_PushStruct(arena, Memmy_ScanResult);
    List_PushBack(&list->list, &result->link);
    return result;
}

Memmy_Status Memmy_Process_ScanPattern(Arena *arena, Memmy_Process *process, Memmy_ScanOptions *options,
                                       Memmy_Pattern pattern, Memmy_ScanResultList *out, Memmy_Error *error)
{
    if (arena == 0 || process == 0 || options == 0 || out == 0)
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

    *out = (Memmy_ScanResultList){0};
    if (options->range.end == options->range.start)
    {
        return Memmy_Status_Ok;
    }
    if (pattern.count == 0 || pattern.bytes == 0)
    {
        Memmy_Error_Set(error, Memmy_Status_InvalidArgument, String8_Lit("scan"), String8_Lit("scan pattern is empty"));
        return Memmy_Status_InvalidArgument;
    }

    B32 any_read = 0;
    B32 stop = 0;
    Memmy_Addr last_match = 0;
    B32 has_last_match = 0;
    Memmy_Backend *backend = process->backend;
    if (backend != 0 && Memmy_Backend_HasCapability(backend, Memmy_BackendCap_ListRegions) &&
        backend->list_regions != 0)
    {
        Scratch scratch = Scratch_Begin(&arena, 1);
        Memmy_RegionList regions = {0};
        Memmy_Status status = Memmy_Process_ListRegions(scratch.arena, process, &regions, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }

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

            status = Memmy_Process_ScanPatternRange(arena, process, scan_range, options, pattern, out, &any_read, &stop,
                                                    &last_match, &has_last_match, error);
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

    Memmy_Status status = Memmy_Process_ScanPatternRange(arena, process, options->range, options, pattern, out,
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
