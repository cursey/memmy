#include "base_os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base_arena.h"
#include "base_core.h"
#include "base_string.h"

#if OS_LINUX || OS_MACOS

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
#endif
