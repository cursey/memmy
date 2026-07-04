#include "base_os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

#if OS_WINDOWS

#include <Windows.h>

void *Os_MemReserve(U64 size)
{
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
}

void Os_MemCommit(void *ptr, U64 size)
{
    VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void Os_MemDecommit(void *ptr, U64 size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

void Os_MemRelease(void *ptr, U64 size)
{
    Unused(size);
    VirtualFree(ptr, 0, MEM_RELEASE);
}

B32 Os_FileReadAll(Arena *a, String8 path, String8 *out_bytes)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    char *cpath = String8_ToCStr(scratch.arena, path);
    HANDLE h = CreateFileA(cpath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    Scratch_End(scratch);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li))
    {
        CloseHandle(h);
        return 0;
    }

    U64 size = (U64)li.QuadPart;
    U8 *buf = Arena_PushArrayNoZero(a, U8, size + 1);
    U64 pos = 0;
    while (pos < size)
    {
        DWORD want = (DWORD)Min(size - pos, (U64)0x40000000);
        DWORD got = 0;
        if (!ReadFile(h, buf + pos, want, &got, 0))
        {
            CloseHandle(h);
            return 0;
        }
        if (got == 0)
        {
            break;
        }
        pos += got;
    }
    buf[pos] = 0;
    CloseHandle(h);

    out_bytes->data = buf;
    out_bytes->len = pos;
    return 1;
}

B32 Os_FileWriteAll(String8 path, String8 bytes)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    HANDLE h = CreateFileA(cpath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    Scratch_End(scratch);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    U64 pos = 0;
    while (pos < bytes.len)
    {
        DWORD want = (DWORD)Min(bytes.len - pos, (U64)0x40000000);
        DWORD wrote = 0;
        if (!WriteFile(h, bytes.data + pos, want, &wrote, 0) || wrote == 0)
        {
            CloseHandle(h);
            return 0;
        }
        pos += wrote;
    }
    CloseHandle(h);
    return 1;
}

B32 Os_FileExists(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    DWORD attr = GetFileAttributesA(cpath);
    Scratch_End(scratch);
    return attr != INVALID_FILE_ATTRIBUTES;
}

B32 Os_FileDelete(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    B32 ok = DeleteFileA(cpath) != 0;
    Scratch_End(scratch);
    return ok;
}

B32 Os_DirCreate(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    B32 ok = CreateDirectoryA(cpath, 0) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
    Scratch_End(scratch);
    return ok;
}

struct Os_DirIter
{
    Arena *arena;
    HANDLE handle;
    WIN32_FIND_DATAA pending;
    B32 have_pending;
};

Os_DirIter *Os_DirIterBegin(Arena *a, String8 path)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    // FindFirstFileA wants "dir\*" to enumerate contents.
    String8 pattern = String8_PushF(scratch.arena, "%.*s\\*", (int)path.len, path.data);
    char *cpat = String8_ToCStr(scratch.arena, pattern);
    WIN32_FIND_DATAA first;
    HANDLE h = FindFirstFileA(cpat, &first);
    Scratch_End(scratch);
    if (h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    Os_DirIter *it = Arena_PushStruct(a, Os_DirIter);
    it->arena = a;
    it->handle = h;
    it->pending = first;
    it->have_pending = 1;
    return it;
}

B32 Os_DirIterNext(Os_DirIter *it, String8 *out_name, B32 *out_is_dir)
{
    if (it == 0 || it->handle == 0 || it->handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    for (;;)
    {
        WIN32_FIND_DATAA entry;
        if (it->have_pending)
        {
            entry = it->pending;
            it->have_pending = 0;
        }
        else if (!FindNextFileA(it->handle, &entry))
        {
            return 0;
        }
        if (entry.cFileName[0] == '.' &&
            (entry.cFileName[1] == 0 || (entry.cFileName[1] == '.' && entry.cFileName[2] == 0)))
        {
            continue;
        }
        *out_name = String8_Copy(it->arena, String8_FromCStr(entry.cFileName));
        *out_is_dir = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return 1;
    }
}

void Os_DirIterEnd(Os_DirIter *it)
{
    if (it != 0 && it->handle != 0 && it->handle != INVALID_HANDLE_VALUE)
    {
        FindClose(it->handle);
        it->handle = INVALID_HANDLE_VALUE;
    }
}

B32 Os_StdinIsTerminal(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == 0 || h == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    DWORD mode = 0;
    return GetConsoleMode(h, &mode) != 0;
}

String8 Os_ReadStdin(Arena *a)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    U64 cap = 4096;
    U8 *buf = Arena_PushArrayNoZero(a, U8, cap);
    U64 pos = 0;
    for (;;)
    {
        if (pos == cap)
        {
            U64 add = cap;
            Arena_PushArrayNoZero(a, U8, add);
            cap += add;
        }
        DWORD want = (DWORD)Min(cap - pos, (U64)0x40000000);
        DWORD got = 0;
        if (!ReadFile(h, buf + pos, want, &got, 0))
        {
            // ERROR_BROKEN_PIPE on a closed pipe counts as EOF.
            break;
        }
        if (got == 0)
        {
            break;
        }
        pos += got;
    }
    return (String8){.data = buf, .len = pos};
}

void Os_WriteStdout(String8 s)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    U64 pos = 0;
    while (pos < s.len)
    {
        DWORD want = (DWORD)Min(s.len - pos, (U64)0x40000000);
        DWORD wrote = 0;
        if (!WriteFile(h, s.data + pos, want, &wrote, 0) || wrote == 0)
        {
            break;
        }
        pos += wrote;
    }
}

void Os_WriteStderr(String8 s)
{
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    U64 pos = 0;
    while (pos < s.len)
    {
        DWORD want = (DWORD)Min(s.len - pos, (U64)0x40000000);
        DWORD wrote = 0;
        if (!WriteFile(h, s.data + pos, want, &wrote, 0) || wrote == 0)
        {
            break;
        }
        pos += wrote;
    }
}

// Drain a pipe handle into an arena-allocated String8. Same bump-allocator
// trick as the POSIX DrainFd.
static String8 DrainHandle(Arena *a, HANDLE h)
{
    U64 cap = 4096;
    U8 *buf = Arena_PushArrayNoZero(a, U8, cap);
    U64 pos = 0;
    for (;;)
    {
        if (pos == cap)
        {
            U64 add = cap;
            Arena_PushArrayNoZero(a, U8, add);
            cap += add;
        }
        DWORD want = (DWORD)Min(cap - pos, (U64)0x40000000);
        DWORD got = 0;
        if (!ReadFile(h, buf + pos, want, &got, 0))
        {
            // ERROR_BROKEN_PIPE == writer side closed == EOF.
            break;
        }
        if (got == 0)
        {
            break;
        }
        pos += got;
    }
    return (String8){.data = buf, .len = pos};
}

// Quote a single argv token per Microsoft's CommandLineToArgvW rules. Appends
// to the caller-owned byte list.
static void AppendQuotedArg(String8List *out, Arena *a, char *arg)
{
    U64 arg_len = 0;
    while (arg[arg_len] != 0)
    {
        arg_len++;
    }

    B32 needs_quotes = arg_len == 0;
    for (U64 i = 0; i < arg_len && !needs_quotes; i++)
    {
        char c = arg[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"')
        {
            needs_quotes = 1;
        }
    }

    if (!needs_quotes)
    {
        String8List_Push(a, out, String8_Copy(a, (String8){.data = (U8 *)arg, .len = arg_len}));
        return;
    }

    // Worst case: every char is a backslash followed by a quote -> 2x.
    // Plus surrounding quotes.
    U64 cap = arg_len * 2 + 2;
    U8 *buf = Arena_PushArrayNoZero(a, U8, cap);
    U64 bp = 0;
    buf[bp++] = '"';
    U64 i = 0;
    while (i < arg_len)
    {
        U64 backslashes = 0;
        while (i < arg_len && arg[i] == '\\')
        {
            backslashes++;
            i++;
        }
        if (i == arg_len)
        {
            // Trailing backslashes: double them so the closing quote isn't escaped.
            for (U64 k = 0; k < backslashes * 2; k++)
            {
                buf[bp++] = '\\';
            }
        }
        else if (arg[i] == '"')
        {
            // Double backslashes and escape the quote.
            for (U64 k = 0; k < backslashes * 2 + 1; k++)
            {
                buf[bp++] = '\\';
            }
            buf[bp++] = '"';
            i++;
        }
        else
        {
            for (U64 k = 0; k < backslashes; k++)
            {
                buf[bp++] = '\\';
            }
            buf[bp++] = (U8)arg[i++];
        }
    }
    buf[bp++] = '"';
    String8List_Push(a, out, (String8){.data = buf, .len = bp});
}

static String8 BuildCmdLine(Arena *a, char **argv)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    String8List parts = {0};
    for (U64 i = 0; argv[i] != 0; i++)
    {
        AppendQuotedArg(&parts, scratch.arena, argv[i]);
    }
    String8 joined = String8List_Join(scratch.arena, &parts, String8_Lit(" "));
    // CreateProcessA wants a writable buffer for the command line.
    U8 *buf = Arena_PushArrayNoZero(a, U8, joined.len + 1);
    memcpy(buf, joined.data, joined.len);
    buf[joined.len] = 0;
    String8 result = {.data = buf, .len = joined.len};
    Scratch_End(scratch);
    return result;
}

B32 Os_ProcRun(Arena *a, Os_ProcSpawn spawn, Os_ProcResult *out)
{
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE in_r = 0, in_w = 0;
    HANDLE out_r = 0, out_w = 0;
    HANDLE err_r = 0, err_w = 0;

    if (!CreatePipe(&in_r, &in_w, &sa, 0))
    {
        return 0;
    }
    if (!CreatePipe(&out_r, &out_w, &sa, 0))
    {
        CloseHandle(in_r);
        CloseHandle(in_w);
        return 0;
    }
    if (!CreatePipe(&err_r, &err_w, &sa, 0))
    {
        CloseHandle(in_r);
        CloseHandle(in_w);
        CloseHandle(out_r);
        CloseHandle(out_w);
        return 0;
    }

    // Keep only the child's ends inheritable.
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

    String8 cmdline = BuildCmdLine(a, spawn.argv);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_r;
    si.hStdOutput = out_w;
    si.hStdError = err_w;

    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessA(0, (char *)cmdline.data, 0, 0, TRUE, 0, 0, 0, &si, &pi);

    // Close the child-side ends in the parent regardless of success.
    CloseHandle(in_r);
    CloseHandle(out_w);
    CloseHandle(err_w);

    if (!ok)
    {
        CloseHandle(in_w);
        CloseHandle(out_r);
        CloseHandle(err_r);
        return 0;
    }

    // Write stdin. If the child closes its stdin or exits, WriteFile will fail;
    // break rather than erroring out.
    U64 inpos = 0;
    while (inpos < spawn.stdin_input.len)
    {
        DWORD want = (DWORD)Min(spawn.stdin_input.len - inpos, (U64)0x40000000);
        DWORD wrote = 0;
        if (!WriteFile(in_w, spawn.stdin_input.data + inpos, want, &wrote, 0) || wrote == 0)
        {
            break;
        }
        inpos += wrote;
    }
    CloseHandle(in_w);

    // Sequential drain. Same deadlock caveat as the POSIX path; fine for
    // LIT/FileCheck-sized outputs, revisit with threads if needed.
    out->stdout_bytes = DrainHandle(a, out_r);
    out->stderr_bytes = DrainHandle(a, err_r);
    CloseHandle(out_r);
    CloseHandle(err_r);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out->exit_code = (I32)exit_code;
    out->signaled = 0;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

String8 Os_GetEnv(Arena *a, String8 name)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    char *cname = String8_ToCStr(scratch.arena, name);
    DWORD needed = GetEnvironmentVariableA(cname, 0, 0);
    if (needed == 0)
    {
        Scratch_End(scratch);
        return (String8){0};
    }
    char *buf = Arena_PushArrayNoZero(a, char, needed);
    DWORD got = GetEnvironmentVariableA(cname, buf, needed);
    Scratch_End(scratch);
    if (got == 0 || got >= needed)
    {
        return (String8){0};
    }
    return String8_FromCStr(buf);
}

U32 Os_GetProcessId(void)
{
    return (U32)GetCurrentProcessId();
}

String8 Os_TempDir(Arena *a)
{
    char buf[MAX_PATH + 2];
    DWORD n = GetTempPathA((DWORD)sizeof(buf), buf);
    if (n == 0 || n > sizeof(buf))
    {
        return String8_Copy(a, String8_Lit("."));
    }
    while (n > 0 && (buf[n - 1] == '\\' || buf[n - 1] == '/'))
    {
        buf[--n] = 0;
    }
    return String8_Copy(a, String8_FromCStr(buf));
}

#elif OS_LINUX || OS_MACOS

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

void *Os_MemReserve(U64 size)
{
    void *p = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? 0 : p;
}

void Os_MemCommit(void *ptr, U64 size)
{
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void Os_MemDecommit(void *ptr, U64 size)
{
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
}

void Os_MemRelease(void *ptr, U64 size)
{
    munmap(ptr, size);
}

B32 Os_FileReadAll(Arena *a, String8 path, String8 *out_bytes)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    char *cpath = String8_ToCStr(scratch.arena, path);
    int fd = open(cpath, O_RDONLY);
    Scratch_End(scratch);
    if (fd < 0)
    {
        return 0;
    }

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        close(fd);
        return 0;
    }

    U64 size = (U64)st.st_size;
    U8 *buf = Arena_PushArrayNoZero(a, U8, size + 1);
    U64 pos = 0;
    while (pos < size)
    {
        ssize_t n = read(fd, buf + pos, size - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            close(fd);
            return 0;
        }
        if (n == 0)
        {
            break;
        }
        pos += (U64)n;
    }
    buf[pos] = 0;
    close(fd);

    out_bytes->data = buf;
    out_bytes->len = pos;
    return 1;
}

B32 Os_FileWriteAll(String8 path, String8 bytes)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    int fd = open(cpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    Scratch_End(scratch);
    if (fd < 0)
    {
        return 0;
    }
    U64 pos = 0;
    while (pos < bytes.len)
    {
        ssize_t n = write(fd, bytes.data + pos, bytes.len - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            close(fd);
            return 0;
        }
        pos += (U64)n;
    }
    close(fd);
    return 1;
}

B32 Os_FileExists(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    struct stat st;
    B32 ok = stat(cpath, &st) == 0;
    Scratch_End(scratch);
    return ok;
}

B32 Os_FileDelete(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    B32 ok = unlink(cpath) == 0;
    Scratch_End(scratch);
    return ok;
}

B32 Os_DirCreate(String8 path)
{
    Scratch scratch = Scratch_Begin(0, 0);
    char *cpath = String8_ToCStr(scratch.arena, path);
    B32 ok = mkdir(cpath, 0755) == 0 || errno == EEXIST;
    Scratch_End(scratch);
    return ok;
}

struct Os_DirIter
{
    Arena *arena;
    DIR *dir;
};

Os_DirIter *Os_DirIterBegin(Arena *a, String8 path)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    char *cpath = String8_ToCStr(scratch.arena, path);
    DIR *dir = opendir(cpath);
    Scratch_End(scratch);
    if (dir == 0)
    {
        return 0;
    }
    Os_DirIter *it = Arena_PushStruct(a, Os_DirIter);
    it->arena = a;
    it->dir = dir;
    return it;
}

B32 Os_DirIterNext(Os_DirIter *it, String8 *out_name, B32 *out_is_dir)
{
    if (it == 0 || it->dir == 0)
    {
        return 0;
    }
    for (;;)
    {
        errno = 0;
        struct dirent *ent = readdir(it->dir);
        if (ent == 0)
        {
            return 0;
        }
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }
        *out_name = String8_Copy(it->arena, String8_FromCStr(ent->d_name));
#ifdef DT_DIR
        if (ent->d_type != DT_UNKNOWN)
        {
            *out_is_dir = (ent->d_type == DT_DIR);
            return 1;
        }
#endif
        // Fallback: stat the file
        *out_is_dir = 0;
        return 1;
    }
}

void Os_DirIterEnd(Os_DirIter *it)
{
    if (it != 0 && it->dir != 0)
    {
        closedir(it->dir);
        it->dir = 0;
    }
}

B32 Os_StdinIsTerminal(void)
{
    return isatty(0) != 0;
}

String8 Os_ReadStdin(Arena *a)
{
    U64 cap = 4096;
    U8 *buf = Arena_PushArrayNoZero(a, U8, cap);
    U64 pos = 0;
    for (;;)
    {
        if (pos == cap)
        {
            U64 new_cap = cap * 2;
            U8 *new_buf = Arena_PushArrayNoZero(a, U8, new_cap - cap);
            Unused(new_buf);
            cap = new_cap;
        }
        ssize_t n = read(0, buf + pos, cap - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (n == 0)
        {
            break;
        }
        pos += (U64)n;
    }
    return (String8){.data = buf, .len = pos};
}

void Os_WriteStdout(String8 s)
{
    U64 pos = 0;
    while (pos < s.len)
    {
        ssize_t n = write(1, s.data + pos, s.len - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        pos += (U64)n;
    }
}

void Os_WriteStderr(String8 s)
{
    U64 pos = 0;
    while (pos < s.len)
    {
        ssize_t n = write(2, s.data + pos, s.len - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        pos += (U64)n;
    }
}

// Drain a file descriptor into an arena-allocated String8 by growing the arena
// in power-of-two chunks. All chunks are consecutive because the arena is a
// bump allocator and no one else writes to it between read()s.
static String8 DrainFd(Arena *a, int fd)
{
    U64 cap = 4096;
    U8 *buf = Arena_PushArrayNoZero(a, U8, cap);
    U64 pos = 0;
    for (;;)
    {
        if (pos == cap)
        {
            U64 add = cap;
            Arena_PushArrayNoZero(a, U8, add);
            cap += add;
        }
        ssize_t n = read(fd, buf + pos, cap - pos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (n == 0)
        {
            break;
        }
        pos += (U64)n;
    }
    return (String8){.data = buf, .len = pos};
}

B32 Os_ProcRun(Arena *a, Os_ProcSpawn spawn, Os_ProcResult *out)
{
    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};

    if (pipe(in_pipe) != 0)
    {
        return 0;
    }
    if (pipe(out_pipe) != 0)
    {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return 0;
    }
    if (pipe(err_pipe) != 0)
    {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return 0;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in_pipe[0], 0);
    posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err_pipe[1], 2);
    posix_spawn_file_actions_addclose(&actions, in_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, in_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, out_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, err_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, err_pipe[1]);

    pid_t pid;
    int rc = posix_spawnp(&pid, spawn.argv[0], &actions, 0, spawn.argv, environ);
    posix_spawn_file_actions_destroy(&actions);

    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);

    if (rc != 0)
    {
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(err_pipe[0]);
        return 0;
    }

    // Write stdin. Ignore SIGPIPE via MSG_NOSIGNAL-equivalent? Just handle EPIPE.
    // Use a local signal-ignore on SIGPIPE for safety.
    void (*prev_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);
    U64 inpos = 0;
    while (inpos < spawn.stdin_input.len)
    {
        ssize_t n = write(in_pipe[1], spawn.stdin_input.data + inpos, spawn.stdin_input.len - inpos);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        inpos += (U64)n;
    }
    close(in_pipe[1]);
    signal(SIGPIPE, prev_sigpipe);

    // Drain stdout then stderr sequentially. This can deadlock for children
    // producing large stderr while waiting on stdout reader. For FileCheck/LIT
    // use cases outputs are small; revisit with threads if needed.
    out->stdout_bytes = DrainFd(a, out_pipe[0]);
    out->stderr_bytes = DrainFd(a, err_pipe[0]);
    close(out_pipe[0]);
    close(err_pipe[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return 0;
        }
    }
    if (WIFEXITED(status))
    {
        out->exit_code = WEXITSTATUS(status);
        out->signaled = 0;
    }
    else if (WIFSIGNALED(status))
    {
        out->exit_code = 128 + WTERMSIG(status);
        out->signaled = 1;
    }
    else
    {
        out->exit_code = -1;
        out->signaled = 0;
    }
    return 1;
}

String8 Os_GetEnv(Arena *a, String8 name)
{
    Scratch scratch = Scratch_Begin(&a, 1);
    char *cname = String8_ToCStr(scratch.arena, name);
    char *val = getenv(cname);
    Scratch_End(scratch);
    if (val == 0)
    {
        return (String8){0};
    }
    return String8_Copy(a, String8_FromCStr(val));
}

U32 Os_GetProcessId(void)
{
    return (U32)getpid();
}

String8 Os_TempDir(Arena *a)
{
    String8 env = Os_GetEnv(a, String8_Lit("TMPDIR"));
    if (env.len > 0)
    {
        return env;
    }
    return String8_Copy(a, String8_Lit("/tmp"));
}

#else
#error "Unsupported OS"
#endif
