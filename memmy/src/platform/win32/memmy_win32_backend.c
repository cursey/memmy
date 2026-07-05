#include "base_core.h"

#if OS_WINDOWS

#include "memmy_win32_backend.h"

#include "base_checked.h"

#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on

typedef struct Memmy_Win32Backend Memmy_Win32Backend;
struct Memmy_Win32Backend
{
    Memmy_Backend backend;
};

typedef struct Memmy_Win32ProcessData Memmy_Win32ProcessData;
struct Memmy_Win32ProcessData
{
    HANDLE handle;
};

static B32 Memmy_Win32_IsNative64(void)
{
    SYSTEM_INFO sys_info;
    GetNativeSystemInfo(&sys_info);
    return sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
           sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
}

static void Memmy_Win32_SetLastError(Memmy_Error *error, Memmy_Status status, String8 context, String8 message)
{
    Memmy_Error_Set(error, status, context, message);
    if (error != 0)
    {
        error->os_code = GetLastError();
    }
}

static String8 Memmy_Win32_CopyCString(Arena *arena, char *text)
{
    return String8_Copy(arena, String8_FromCStr(text));
}

static Memmy_PointerWidth Memmy_Win32_QueryPointerWidth(HANDLE process)
{
    if (!Memmy_Win32_IsNative64())
    {
        return Memmy_PointerWidth_32;
    }

    BOOL wow64 = FALSE;
    if (IsWow64Process(process, &wow64) && wow64)
    {
        return Memmy_PointerWidth_32;
    }
    return Memmy_PointerWidth_64;
}

static B32 Memmy_Win32_IsUnsupportedCrossBitness(Memmy_PointerWidth target_width)
{
    return sizeof(void *) == 4 && Memmy_Win32_IsNative64() && target_width == Memmy_PointerWidth_64;
}

static Memmy_Status Memmy_Win32_EnumerateProcesses(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        Memmy_Win32_SetLastError(error, Memmy_Status_PlatformError, String8_Lit("win32"),
                                 String8_Lit("CreateToolhelp32Snapshot failed"));
        return Memmy_Status_PlatformError;
    }

    PROCESSENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry))
    {
        DWORD err = GetLastError();
        CloseHandle(snapshot);
        if (err == ERROR_NO_MORE_FILES)
        {
            return Memmy_Status_Ok;
        }
        Memmy_Error_Set(error, Memmy_Status_PlatformError, String8_Lit("win32"), String8_Lit("Process32First failed"));
        if (error != 0)
        {
            error->os_code = err;
        }
        return Memmy_Status_PlatformError;
    }

    do
    {
        Memmy_ProcessInfo info = {
            .pid = entry.th32ProcessID,
            .name = Memmy_Win32_CopyCString(arena, entry.szExeFile),
            .path = {0},
            .pointer_width = Memmy_PointerWidth_Unknown,
        };

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process != 0)
        {
            char path[MAX_PATH];
            DWORD path_size = ArrayCount(path);
            if (QueryFullProcessImageNameA(process, 0, path, &path_size))
            {
                info.path = String8_Copy(arena, String8_Make((U8 *)path, path_size));
            }
            info.pointer_width = Memmy_Win32_QueryPointerWidth(process);
            CloseHandle(process);
        }

        Memmy_Status status = sink.callback(sink.user_data, &info);
        if (status != Memmy_Status_Ok)
        {
            CloseHandle(snapshot);
            return status;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return Memmy_Status_Ok;
}

static DWORD Memmy_Win32_ProcessAccess(void)
{
    return PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
           PROCESS_VM_OPERATION;
}

static Memmy_Status Memmy_Win32_OpenProcess(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    HANDLE handle = OpenProcess(Memmy_Win32_ProcessAccess(), FALSE, pid);
    if (handle == 0)
    {
        DWORD err = GetLastError();
        Memmy_Status status = (err == ERROR_ACCESS_DENIED) ? Memmy_Status_AccessDenied : Memmy_Status_NotFound;
        Memmy_Error_Set(error, status, String8_Lit("win32"), String8_Lit("OpenProcess failed"));
        if (error != 0)
        {
            error->os_code = err;
        }
        return status;
    }

    Memmy_PointerWidth pointer_width = Memmy_Win32_QueryPointerWidth(handle);
    if (Memmy_Win32_IsUnsupportedCrossBitness(pointer_width))
    {
        CloseHandle(handle);
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("win32"),
                        String8_Lit("32-bit Memmy cannot inspect a 64-bit target process"));
        return Memmy_Status_Unsupported;
    }

    Memmy_Win32Backend *backend = ContainerOf(Memmy_Context_Get()->backend, Memmy_Win32Backend, backend);
    Memmy_Win32ProcessData *data = Arena_PushStruct(arena, Memmy_Win32ProcessData);
    data->handle = handle;

    Memmy_Process *process = Arena_PushStruct(arena, Memmy_Process);
    process->backend = &backend->backend;
    process->pid = pid;
    process->pointer_width = pointer_width;
    process->backend_data = data;
    *out = process;
    return Memmy_Status_Ok;
}

static void Memmy_Win32_CloseProcess(Memmy_Process *process)
{
    Memmy_Win32ProcessData *data = (Memmy_Win32ProcessData *)process->backend_data;
    if (data != 0 && data->handle != 0)
    {
        CloseHandle(data->handle);
        data->handle = 0;
    }
    process->backend_data = 0;
}

static Memmy_Status Memmy_Win32_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                                     Memmy_Error *error)
{
    Memmy_Win32ProcessData *data = (Memmy_Win32ProcessData *)process->backend_data;
    SIZE_T got = 0;
    *bytes_read = 0;

    if (size > (U64)SIZE_MAX)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("win32"), String8_Lit("read size overflow"));
        return Memmy_Status_Overflow;
    }

    BOOL ok = ReadProcessMemory(data->handle, (void *)(uintptr_t)addr, buffer, (SIZE_T)size, &got);
    *bytes_read = (U64)got;
    if (!ok)
    {
        DWORD err = GetLastError();
        Memmy_Status status = Memmy_Status_PlatformError;
        if (got > 0)
        {
            status = Memmy_Status_PartialRead;
        }
        else if (err == ERROR_ACCESS_DENIED)
        {
            status = Memmy_Status_AccessDenied;
        }
        else if (err == ERROR_PARTIAL_COPY || err == ERROR_INVALID_ADDRESS || err == ERROR_NOACCESS)
        {
            status = Memmy_Status_Unreadable;
        }
        Memmy_Error_Set(error, status, String8_Lit("win32"), String8_Lit("ReadProcessMemory failed"));
        if (error != 0)
        {
            error->os_code = err;
            error->byte_count = (U64)got;
        }
        return status;
    }
    if (got != (SIZE_T)size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("win32"), String8_Lit("partial read"));
        if (error != 0)
        {
            error->byte_count = (U64)got;
        }
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Win32_Write(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
                                      U64 *bytes_written, Memmy_Error *error)
{
    Memmy_Win32ProcessData *data = (Memmy_Win32ProcessData *)process->backend_data;
    SIZE_T got = 0;
    *bytes_written = 0;

    if (size > (U64)SIZE_MAX)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("win32"), String8_Lit("write size overflow"));
        return Memmy_Status_Overflow;
    }

    BOOL ok = WriteProcessMemory(data->handle, (void *)(uintptr_t)addr, buffer, (SIZE_T)size, &got);
    *bytes_written = (U64)got;
    if (!ok)
    {
        DWORD err = GetLastError();
        Memmy_Status status = Memmy_Status_PlatformError;
        if (got > 0)
        {
            status = Memmy_Status_PartialWrite;
        }
        else if (err == ERROR_ACCESS_DENIED)
        {
            status = Memmy_Status_AccessDenied;
        }
        else if (err == ERROR_PARTIAL_COPY || err == ERROR_INVALID_ADDRESS || err == ERROR_NOACCESS)
        {
            status = Memmy_Status_Unwritable;
        }
        Memmy_Error_Set(error, status, String8_Lit("win32"), String8_Lit("WriteProcessMemory failed"));
        if (error != 0)
        {
            error->os_code = err;
            error->byte_count = (U64)got;
        }
        return status;
    }
    if (got != (SIZE_T)size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialWrite, String8_Lit("win32"), String8_Lit("partial write"));
        if (error != 0)
        {
            error->byte_count = (U64)got;
        }
        return Memmy_Status_PartialWrite;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Win32_EnumerateModules(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink,
                                                 Memmy_Error *error)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process->pid);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        Memmy_Win32_SetLastError(error, Memmy_Status_PlatformError, String8_Lit("win32"),
                                 String8_Lit("module snapshot failed"));
        return Memmy_Status_PlatformError;
    }

    MODULEENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);
    if (!Module32First(snapshot, &entry))
    {
        DWORD err = GetLastError();
        CloseHandle(snapshot);
        if (err == ERROR_NO_MORE_FILES)
        {
            return Memmy_Status_Ok;
        }
        Memmy_Error_Set(error, Memmy_Status_PlatformError, String8_Lit("win32"), String8_Lit("Module32First failed"));
        if (error != 0)
        {
            error->os_code = err;
        }
        return Memmy_Status_PlatformError;
    }

    do
    {
        Memmy_Module module = {
            .name = Memmy_Win32_CopyCString(arena, entry.szModule),
            .path = Memmy_Win32_CopyCString(arena, entry.szExePath),
            .base = (Memmy_Addr)(uintptr_t)entry.modBaseAddr,
            .size = entry.modBaseSize,
        };
        Memmy_Status status = sink.callback(sink.user_data, &module);
        if (status != Memmy_Status_Ok)
        {
            CloseHandle(snapshot);
            return status;
        }
    } while (Module32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return Memmy_Status_Ok;
}

static Memmy_RegionState Memmy_Win32_RegionState(DWORD state)
{
    if (state == MEM_COMMIT)
    {
        return Memmy_RegionState_Committed;
    }
    if (state == MEM_RESERVE)
    {
        return Memmy_RegionState_Reserved;
    }
    return Memmy_RegionState_Free;
}

static Memmy_RegionAccess Memmy_Win32_RegionAccess(DWORD protect)
{
    Memmy_RegionAccess result = 0;
    if (protect & PAGE_GUARD)
    {
        result |= Memmy_RegionAccess_Guard;
    }

    protect &= 0xff;
    switch (protect)
    {
    case PAGE_READONLY:
        result |= Memmy_RegionAccess_Read;
        break;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
        result |= Memmy_RegionAccess_Read | Memmy_RegionAccess_Write;
        break;
    case PAGE_EXECUTE:
        result |= Memmy_RegionAccess_Execute;
        break;
    case PAGE_EXECUTE_READ:
        result |= Memmy_RegionAccess_Read | Memmy_RegionAccess_Execute;
        break;
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        result |= Memmy_RegionAccess_Read | Memmy_RegionAccess_Write | Memmy_RegionAccess_Execute;
        break;
    }
    return result;
}

static Memmy_Status Memmy_Win32_EnumerateRegions(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink,
                                                 Memmy_Error *error)
{
    Unused(arena);

    Memmy_Win32ProcessData *data = (Memmy_Win32ProcessData *)process->backend_data;
    U8 *addr = 0;
    for (;;)
    {
        MEMORY_BASIC_INFORMATION mbi = {0};
        SIZE_T got = VirtualQueryEx(data->handle, addr, &mbi, sizeof(mbi));
        if (got == 0)
        {
            break;
        }

        Memmy_Region region = {
            .base = (Memmy_Addr)(uintptr_t)mbi.BaseAddress,
            .size = (Memmy_Size)mbi.RegionSize,
            .access = Memmy_Win32_RegionAccess(mbi.Protect),
            .state = Memmy_Win32_RegionState(mbi.State),
        };

        U64 next = 0;
        if (!AddU64Checked(region.base, region.size, &next) || next <= region.base)
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("win32"), String8_Lit("region end overflow"));
            return Memmy_Status_Overflow;
        }
        Memmy_Status status = sink.callback(sink.user_data, &region);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        addr = (U8 *)(uintptr_t)next;
    }

    return Memmy_Status_Ok;
}

Memmy_Backend *Memmy_Win32Backend_Create(Arena *arena)
{
    Memmy_Win32Backend *backend = Arena_PushStruct(arena, Memmy_Win32Backend);
    backend->backend = (Memmy_Backend){
        .name = String8_Lit("win32"),
        .enumerate_processes = Memmy_Win32_EnumerateProcesses,
        .open_process = Memmy_Win32_OpenProcess,
        .close_process = Memmy_Win32_CloseProcess,
        .read = Memmy_Win32_Read,
        .write = Memmy_Win32_Write,
        .enumerate_modules = Memmy_Win32_EnumerateModules,
        .enumerate_regions = Memmy_Win32_EnumerateRegions,
    };
    return &backend->backend;
}

#endif
