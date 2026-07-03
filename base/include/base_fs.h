#ifndef BASE_FS_H
#define BASE_FS_H

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

// ---------------------------------------------------------------------------
// Filesystem
// ---------------------------------------------------------------------------

B32 Fs_ReadFile(Arena *a, String8 path, String8 *out);
B32 Fs_WriteFile(String8 path, String8 contents);

// ---------------------------------------------------------------------------
// Path manipulation (pure string ops; handles both '/' and '\\')
// ---------------------------------------------------------------------------

String8 Fs_PathJoin(Arena *a, String8 base, String8 rel);
String8 Fs_PathBasename(String8 path);
String8 Fs_PathDirname(String8 path);
String8 Fs_PathExtension(String8 path); // includes the leading '.'
String8 Fs_PathStripExt(String8 path);

// ---------------------------------------------------------------------------
// Directory walk
//
// Recursively collects all regular-file paths under `root`. If `ext_filter`
// is non-empty, only files whose extension equals it (case-sensitive, must
// include the leading '.') are returned.
// ---------------------------------------------------------------------------

String8Slice Fs_Walk(Arena *a, String8 root, String8 ext_filter);

// ---------------------------------------------------------------------------
// Temporary files
// ---------------------------------------------------------------------------

String8 Fs_TempFile(Arena *a, String8 hint);

#endif // BASE_FS_H
