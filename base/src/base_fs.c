#include "base_fs.h"

#include "base_arena.h"
#include "base_core.h"
#include "base_list.h"
#include "base_os.h"
#include "base_string.h"

B32 Fs_ReadFile(Arena *a, String8 path, String8 *out)
{
    return Os_FileReadAll(a, path, out);
}

B32 Fs_WriteFile(String8 path, String8 contents)
{
    return Os_FileWriteAll(path, contents);
}

// ---------------------------------------------------------------------------
// Path manipulation
// ---------------------------------------------------------------------------

static B32 IsPathSep(U8 c)
{
    return c == '/' || c == '\\';
}

static U64 FindLastSep(String8 path)
{
    for (U64 i = path.len; i > 0; i--)
    {
        if (IsPathSep(path.data[i - 1]))
        {
            return i - 1;
        }
    }
    return STRING8_NPOS;
}

String8 Fs_PathJoin(Arena *a, String8 base, String8 rel)
{
    if (base.len == 0)
    {
        return String8_Copy(a, rel);
    }
    if (rel.len == 0)
    {
        return String8_Copy(a, base);
    }
    B32 base_has_sep = IsPathSep(base.data[base.len - 1]);
    B32 rel_has_sep = IsPathSep(rel.data[0]);
    if (base_has_sep && rel_has_sep)
    {
        return String8_PushF(a, "%.*s%.*s", (int)(base.len - 1), base.data, (int)rel.len, rel.data);
    }
    if (base_has_sep || rel_has_sep)
    {
        return String8_PushF(a, "%.*s%.*s", (int)base.len, base.data, (int)rel.len, rel.data);
    }
    return String8_PushF(a, "%.*s/%.*s", (int)base.len, base.data, (int)rel.len, rel.data);
}

String8 Fs_PathBasename(String8 path)
{
    U64 sep = FindLastSep(path);
    if (sep == STRING8_NPOS)
    {
        return path;
    }
    return String8_Substr(path, sep + 1, path.len - sep - 1);
}

String8 Fs_PathDirname(String8 path)
{
    U64 sep = FindLastSep(path);
    if (sep == STRING8_NPOS)
    {
        return String8_Lit(".");
    }
    if (sep == 0)
    {
        return String8_Substr(path, 0, 1);
    }
    return String8_Substr(path, 0, sep);
}

String8 Fs_PathExtension(String8 path)
{
    String8 base = Fs_PathBasename(path);
    U64 dot = String8_FindLastChar(base, '.');
    if (dot == STRING8_NPOS || dot == 0)
    {
        return (String8){0};
    }
    return String8_Substr(base, dot, base.len - dot);
}

String8 Fs_PathStripExt(String8 path)
{
    String8 ext = Fs_PathExtension(path);
    if (ext.len == 0)
    {
        return path;
    }
    return String8_Substr(path, 0, path.len - ext.len);
}

// ---------------------------------------------------------------------------
// Walk
// ---------------------------------------------------------------------------

typedef struct WalkEntry WalkEntry;
struct WalkEntry
{
    ListLink link;
    String8 path;
};

static void WalkInto(Arena *out_arena, Arena *scratch_arena, String8 dir, String8 ext_filter, List *acc)
{
    Os_DirIter *it = Os_DirIterBegin(scratch_arena, dir);
    if (it == 0)
    {
        return;
    }
    String8 name;
    B32 is_dir;
    while (Os_DirIterNext(it, &name, &is_dir))
    {
        String8 sub = Fs_PathJoin(scratch_arena, dir, name);
        if (is_dir)
        {
            WalkInto(out_arena, scratch_arena, sub, ext_filter, acc);
        }
        else
        {
            if (ext_filter.len > 0)
            {
                String8 ext = Fs_PathExtension(sub);
                if (!String8_Eq(ext, ext_filter))
                {
                    continue;
                }
            }
            WalkEntry *entry = Arena_PushStruct(out_arena, WalkEntry);
            entry->path = String8_Copy(out_arena, sub);
            List_PushBack(acc, &entry->link);
        }
    }
    Os_DirIterEnd(it);
}

String8Slice Fs_Walk(Arena *a, String8 root, String8 ext_filter)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    List acc = {0};
    WalkInto(a, scratch.arena, root, ext_filter, &acc);

    String8Slice slice;
    slice.count = acc.count;
    slice.v = Arena_PushArrayNoZero(a, String8, slice.count);
    U64 i = 0;
    List_ForEach(WalkEntry, e, &acc, link)
    {
        slice.v[i++] = e->path;
    }
    Scratch_End(scratch);
    return slice;
}

// ---------------------------------------------------------------------------
// Temp files
// ---------------------------------------------------------------------------

String8 Fs_TempFile(Arena *a, String8 hint)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    String8 tmpdir = Os_TempDir(scratch.arena);
    U32 pid = Os_GetProcessId();
    static U64 counter;
    counter++;
    String8 hint_base = hint.len > 0 ? hint : String8_Lit("liftir");
    String8 result = String8_PushF(a, "%.*s/%.*s.%u.%llu", (int)tmpdir.len, tmpdir.data, (int)hint_base.len,
                                   hint_base.data, pid, (unsigned long long)counter);
    Scratch_End(scratch);
    return result;
}
