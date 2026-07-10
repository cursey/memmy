#include "base_core.h"

#if OS_WINDOWS

#include "memmy_win32_backend.h"

#include "base_checked.h"

#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#include <stddef.h>

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

typedef struct Memmy_Win32ModuleSearch Memmy_Win32ModuleSearch;
struct Memmy_Win32ModuleSearch
{
    Memmy_Addr address;
    Memmy_Module module;
    B32 found;
    Memmy_Error *error;
};

typedef struct Memmy_Win32RuntimeFunctionEntry Memmy_Win32RuntimeFunctionEntry;
struct Memmy_Win32RuntimeFunctionEntry
{
    U32 begin_address;
    U32 end_address;
    U32 unwind_info_address;
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

static Memmy_Status Memmy_Win32_Write(Memmy_Process *process, Memmy_Addr addr, void const *buffer, U64 size,
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

static Memmy_Status Memmy_Win32_ReadExact(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
                                          Memmy_Error *error)
{
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Process_Read(process, addr, buffer, size, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (bytes_read != size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("win32"), String8_Lit("partial PE metadata read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Win32_ModuleSearch_Push(void *user_data, Memmy_Module const *module)
{
    Memmy_Win32ModuleSearch *search = (Memmy_Win32ModuleSearch *)user_data;
    Memmy_Addr end = 0;
    if (!AddU64Checked(module->base, module->size, &end))
    {
        Memmy_Error_Set(search->error, Memmy_Status_Overflow, String8_Lit("function"),
                        String8_Lit("module end overflow"));
        return Memmy_Status_Overflow;
    }

    if (search->address >= module->base && search->address < end)
    {
        search->module = *module;
        search->found = 1;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Win32_RvaToVa(Memmy_Module module, U32 rva, Memmy_Addr *out, Memmy_Error *error)
{
    if ((U64)rva > module.size)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("function"),
                        String8_Lit("function metadata not found"));
        return Memmy_Status_NotFound;
    }
    if (!AddU64Checked(module.base, (U64)rva, out))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("RVA overflow"));
        return Memmy_Status_Overflow;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Win32_FunctionMetadataNotFound(Memmy_Error *error)
{
    Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("function"), String8_Lit("function metadata not found"));
    return Memmy_Status_NotFound;
}

static Memmy_Status Memmy_Win32_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address, Memmy_Range *out,
                                             Memmy_Error *error)
{
    Memmy_Win32ModuleSearch search = {
        .address = address,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_Win32_ModuleSearch_Push,
        .user_data = &search,
    };
    Memmy_Status status = Memmy_Win32_EnumerateModules(arena, process, sink, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!search.found)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    Memmy_Module module = search.module;
    IMAGE_DOS_HEADER dos = {0};
    status = Memmy_Win32_ReadExact(process, module.base, &dos, sizeof(dos), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    Memmy_Addr nt = 0;
    if (!AddU64Checked(module.base, (U64)dos.e_lfanew, &nt))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("PE header overflow"));
        return Memmy_Status_Overflow;
    }

    DWORD signature = 0;
    status = Memmy_Win32_ReadExact(process, nt, &signature, sizeof(signature), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (signature != IMAGE_NT_SIGNATURE)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    Memmy_Addr file_header_addr = 0;
    if (!AddU64Checked(nt, sizeof(signature), &file_header_addr))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("PE header overflow"));
        return Memmy_Status_Overflow;
    }

    IMAGE_FILE_HEADER file_header = {0};
    status = Memmy_Win32_ReadExact(process, file_header_addr, &file_header, sizeof(file_header), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    Memmy_Addr optional_header_addr = 0;
    if (!AddU64Checked(file_header_addr, sizeof(file_header), &optional_header_addr))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("PE header overflow"));
        return Memmy_Status_Overflow;
    }

    WORD optional_magic = 0;
    status = Memmy_Win32_ReadExact(process, optional_header_addr, &optional_magic, sizeof(optional_magic), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    U64 exception_dir_offset = offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory) +
                               sizeof(IMAGE_DATA_DIRECTORY) * IMAGE_DIRECTORY_ENTRY_EXCEPTION;
    U64 exception_dir_end = exception_dir_offset + sizeof(IMAGE_DATA_DIRECTORY);
    if ((U64)file_header.SizeOfOptionalHeader < exception_dir_end)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    Memmy_Addr exception_dir_addr = 0;
    if (!AddU64Checked(optional_header_addr, exception_dir_offset, &exception_dir_addr))
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("PE header overflow"));
        return Memmy_Status_Overflow;
    }

    IMAGE_DATA_DIRECTORY exception_dir = {0};
    status = Memmy_Win32_ReadExact(process, exception_dir_addr, &exception_dir, sizeof(exception_dir), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (exception_dir.VirtualAddress == 0 || exception_dir.Size < sizeof(Memmy_Win32RuntimeFunctionEntry) ||
        exception_dir.Size % sizeof(Memmy_Win32RuntimeFunctionEntry) != 0)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }
    if ((U64)exception_dir.VirtualAddress > module.size || (U64)exception_dir.Size > module.size ||
        (U64)exception_dir.VirtualAddress > module.size - (U64)exception_dir.Size)
    {
        return Memmy_Win32_FunctionMetadataNotFound(error);
    }

    Memmy_Addr table_addr = 0;
    status = Memmy_Win32_RvaToVa(module, exception_dir.VirtualAddress, &table_addr, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }

    U64 entry_count = exception_dir.Size / sizeof(Memmy_Win32RuntimeFunctionEntry);
    for (U64 i = 0; i < entry_count; i++)
    {
        U64 entry_offset = 0;
        if (i > U64_MAX / sizeof(Memmy_Win32RuntimeFunctionEntry))
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                            String8_Lit("function table offset overflow"));
            return Memmy_Status_Overflow;
        }
        entry_offset = i * sizeof(Memmy_Win32RuntimeFunctionEntry);

        Memmy_Addr entry_addr = 0;
        if (!AddU64Checked(table_addr, entry_offset, &entry_addr))
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                            String8_Lit("function table address overflow"));
            return Memmy_Status_Overflow;
        }

        Memmy_Win32RuntimeFunctionEntry entry = {0};
        status = Memmy_Win32_ReadExact(process, entry_addr, &entry, sizeof(entry), error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }

        Memmy_Addr start = 0;
        status = Memmy_Win32_RvaToVa(module, entry.begin_address, &start, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        Memmy_Addr end = 0;
        status = Memmy_Win32_RvaToVa(module, entry.end_address, &end, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (end < start)
        {
            return Memmy_Win32_FunctionMetadataNotFound(error);
        }
        if (address >= start && address < end)
        {
            return Memmy_Range_FromStartEnd(start, end, out, error);
        }
    }

    return Memmy_Win32_FunctionMetadataNotFound(error);
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

static B32 Memmy_Win32_IsReadableCommitted(MEMORY_BASIC_INFORMATION *mbi)
{
    Memmy_RegionAccess access = Memmy_Win32_RegionAccess(mbi->Protect);
    return mbi->State == MEM_COMMIT && (access & Memmy_RegionAccess_Read) != 0 &&
           (access & Memmy_RegionAccess_Guard) == 0;
}

static B32 Memmy_Win32_IsExecutableCommitted(MEMORY_BASIC_INFORMATION *mbi)
{
    Memmy_RegionAccess access = Memmy_Win32_RegionAccess(mbi->Protect);
    return mbi->State == MEM_COMMIT && (access & Memmy_RegionAccess_Execute) != 0 &&
           (access & Memmy_RegionAccess_Guard) == 0;
}

static B32 Memmy_Win32_QueryAddress(Memmy_Process *process, Memmy_Addr address, MEMORY_BASIC_INFORMATION *out)
{
    Memmy_Win32ProcessData *data = (Memmy_Win32ProcessData *)process->backend_data;
    SIZE_T got = VirtualQueryEx(data->handle, (void *)(uintptr_t)address, out, sizeof(*out));
    return got != 0;
}

static B32 Memmy_Win32_AddressInRegion(MEMORY_BASIC_INFORMATION *mbi, Memmy_Addr address)
{
    Memmy_Addr base = (Memmy_Addr)(uintptr_t)mbi->BaseAddress;
    Memmy_Addr end = base + (Memmy_Size)mbi->RegionSize;
    return address >= base && address < end && end >= base;
}

static B32 Memmy_Win32_ReadPointer(Memmy_Process *process, Memmy_Addr address, Memmy_Addr *out)
{
    Memmy_Error ignored = {0};
    if (process->pointer_width == Memmy_PointerWidth_32)
    {
        U32 value = 0;
        if (Memmy_Win32_ReadExact(process, address, &value, sizeof(value), &ignored) != Memmy_Status_Ok)
        {
            return 0;
        }
        *out = value;
    }
    else
    {
        U64 value = 0;
        if (Memmy_Win32_ReadExact(process, address, &value, sizeof(value), &ignored) != Memmy_Status_Ok)
        {
            return 0;
        }
        *out = value;
    }
    return 1;
}

static B32 Memmy_Win32_IsPlausibleVtable(Memmy_Process *process, Memmy_Addr vtable, U32 min_vtable_entries)
{
    MEMORY_BASIC_INFORMATION vtable_mbi = {0};
    if (!Memmy_Win32_QueryAddress(process, vtable, &vtable_mbi) || !Memmy_Win32_IsReadableCommitted(&vtable_mbi))
    {
        return 0;
    }

    U64 pointer_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    for (U32 i = 0; i < min_vtable_entries; i++)
    {
        Memmy_Addr entry_addr = vtable + (U64)i * pointer_size;
        if (!Memmy_Win32_AddressInRegion(&vtable_mbi, entry_addr))
        {
            return 0;
        }

        Memmy_Addr function_addr = 0;
        if (!Memmy_Win32_ReadPointer(process, entry_addr, &function_addr))
        {
            return 0;
        }

        MEMORY_BASIC_INFORMATION function_mbi = {0};
        if (!Memmy_Win32_QueryAddress(process, function_addr, &function_mbi) ||
            !Memmy_Win32_IsExecutableCommitted(&function_mbi))
        {
            return 0;
        }
    }
    return 1;
}

static Memmy_Status Memmy_Win32_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                               Memmy_ObjectBaseOptions const *options, Memmy_ObjectBaseResult *out,
                                               Memmy_Error *error)
{
    Unused(arena);

    MEMORY_BASIC_INFORMATION mbi = {0};
    if (!Memmy_Win32_QueryAddress(process, address, &mbi) || !Memmy_Win32_IsReadableCommitted(&mbi))
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("objectbase"),
                        String8_Lit("object base metadata not found"));
        return Memmy_Status_NotFound;
    }

    U64 pointer_size = process->pointer_width == Memmy_PointerWidth_32 ? 4 : 8;
    Memmy_Addr region_base = (Memmy_Addr)(uintptr_t)mbi.BaseAddress;
    Memmy_Addr region_end = region_base + (Memmy_Size)mbi.RegionSize;
    Memmy_Addr scan_min = address > options->max_scan_back ? address - options->max_scan_back : 0;
    scan_min = Max(scan_min, region_base);
    Memmy_Addr candidate = address - (address % pointer_size);
    B32 found = 0;
    B32 ambiguous = 0;
    Memmy_ObjectBaseResult best = {0};

    for (;;)
    {
        if (candidate < scan_min || candidate + pointer_size > region_end)
        {
            break;
        }

        Memmy_Addr vtable = 0;
        if (Memmy_Win32_ReadPointer(process, candidate, &vtable) &&
            Memmy_Win32_IsPlausibleVtable(process, vtable, options->min_vtable_entries))
        {
            Memmy_ObjectBaseResult result = {
                .address = candidate,
                .vptr_address = candidate,
                .vtable = vtable,
                .confidence = Memmy_ObjectBaseConfidence_Weak,
            };
            if (!found)
            {
                best = result;
                found = 1;
            }
            else if (best.confidence == result.confidence)
            {
                ambiguous = 1;
            }
        }

        if (candidate < pointer_size)
        {
            break;
        }
        candidate -= pointer_size;
    }

    if (!found)
    {
        Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("objectbase"),
                        String8_Lit("object base metadata not found"));
        return Memmy_Status_NotFound;
    }
    if (ambiguous)
    {
        Memmy_Error_Set(error, Memmy_Status_Ambiguous, String8_Lit("objectbase"),
                        String8_Lit("multiple object base candidates found"));
        return Memmy_Status_Ambiguous;
    }

    *out = best;
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
        .find_function = Memmy_Win32_FindFunction,
        .find_object_base = Memmy_Win32_FindObjectBase,
    };
    return &backend->backend;
}

#endif
