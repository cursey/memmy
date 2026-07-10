#include "base_core.h"

#if OS_MACOS

#include "memmy_darwin_backend.h"

#include "base_checked.h"
#include "base_memory.h"
#include "base_os.h"
#include "memmy_range.h"

#include <errno.h>
#include <libproc.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

typedef struct Memmy_DarwinBackend Memmy_DarwinBackend;
struct Memmy_DarwinBackend
{
    Memmy_Backend backend;
};

typedef struct Memmy_DarwinProcessData Memmy_DarwinProcessData;
struct Memmy_DarwinProcessData
{
    mach_port_t task;
    B32 owns_task;
};

typedef struct Memmy_DarwinModuleSearch Memmy_DarwinModuleSearch;
struct Memmy_DarwinModuleSearch
{
    Memmy_Addr address;
    Memmy_Module module;
    B32 found;
    Memmy_Error *error;
};

typedef struct Memmy_DarwinSegmentInfo Memmy_DarwinSegmentInfo;
struct Memmy_DarwinSegmentInfo
{
    U64 vmaddr;
    U64 vmsize;
    U64 fileoff;
    U64 filesize;
};

static String8 Memmy_Darwin_Basename(String8 path);

static void Memmy_Darwin_SetError(Memmy_Error *error, Memmy_Status status, String8 message, kern_return_t kr)
{
    Memmy_Error_Set(error, status, String8_Lit("darwin"), message);
    if (error != 0)
    {
        error->os_code = (U32)kr;
    }
}

static Memmy_Status Memmy_Darwin_StatusFromKern(kern_return_t kr, Memmy_Status invalid_status)
{
    Memmy_Status status = Memmy_Status_PlatformError;
    switch (kr)
    {
    case KERN_SUCCESS:
        status = Memmy_Status_Ok;
        break;
    case KERN_INVALID_ADDRESS:
        status = invalid_status;
        break;
    case KERN_PROTECTION_FAILURE:
        status = Memmy_Status_AccessDenied;
        break;
    case KERN_NO_ACCESS:
        status = Memmy_Status_AccessDenied;
        break;
    case KERN_INVALID_ARGUMENT:
        status = Memmy_Status_InvalidArgument;
        break;
    default:
        break;
    }
    return status;
}

static String8 Memmy_Darwin_CopyCString(Arena *arena, char *text)
{
    return String8_Copy(arena, String8_FromCStr(text));
}

static Memmy_PointerWidth Memmy_Darwin_PointerWidth(void)
{
    return sizeof(void *) == 8 ? Memmy_PointerWidth_64 : Memmy_PointerWidth_32;
}

static Memmy_Status Memmy_Darwin_EnumerateProcesses(Arena *arena, Memmy_ProcessInfoSink sink, Memmy_Error *error)
{
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, 0, 0);
    if (bytes <= 0)
    {
        Memmy_Error_Set(error, Memmy_Status_PlatformError, String8_Lit("darwin"), String8_Lit("proc_listpids failed"));
        if (error != 0)
        {
            error->os_code = (U32)errno;
        }
        return Memmy_Status_PlatformError;
    }

    U64 pid_count = ((U64)bytes / sizeof(pid_t)) + 64;
    pid_t *pids = Arena_PushArray(arena, pid_t, pid_count);
    bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, (int)(pid_count * sizeof(pid_t)));
    if (bytes <= 0)
    {
        Memmy_Error_Set(error, Memmy_Status_PlatformError, String8_Lit("darwin"), String8_Lit("proc_listpids failed"));
        if (error != 0)
        {
            error->os_code = (U32)errno;
        }
        return Memmy_Status_PlatformError;
    }

    U64 got_count = (U64)bytes / sizeof(pid_t);
    for (U64 i = 0; i < got_count; i++)
    {
        pid_t pid = pids[i];
        if (pid <= 0)
        {
            continue;
        }

        char name[PROC_PIDPATHINFO_MAXSIZE] = {0};
        proc_name(pid, name, sizeof(name));

        char path[PROC_PIDPATHINFO_MAXSIZE] = {0};
        int path_len = proc_pidpath(pid, path, sizeof(path));

        if (name[0] == 0 && path_len <= 0)
        {
            continue;
        }

        String8 path_string =
            path_len > 0 ? String8_Copy(arena, String8_Make((U8 *)path, (U64)path_len)) : (String8){0};

        Memmy_ProcessInfo info = {
            .pid = (U32)pid,
            .path = path_string,
            .name = name[0] != 0 ? Memmy_Darwin_CopyCString(arena, name)
                                 : String8_Copy(arena, Memmy_Darwin_Basename(path_string)),
            .pointer_width = Memmy_Darwin_PointerWidth(),
        };
        Memmy_Status status = sink.callback(sink.user_data, &info);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_OpenProcess(Arena *arena, U32 pid, Memmy_Process **out, Memmy_Error *error)
{
    mach_port_t task = MACH_PORT_NULL;
    B32 owns_task = 0;
    if (pid == Os_GetProcessId())
    {
        task = mach_task_self();
    }
    else
    {
        kern_return_t kr = task_for_pid(mach_task_self(), (pid_t)pid, &task);
        if (kr != KERN_SUCCESS)
        {
            Memmy_Status status =
                kr == KERN_FAILURE ? Memmy_Status_AccessDenied : Memmy_Darwin_StatusFromKern(kr, Memmy_Status_NotFound);
            Memmy_Darwin_SetError(error, status, String8_Lit("task_for_pid failed"), kr);
            return status;
        }
        owns_task = 1;
    }

    Memmy_DarwinBackend *backend = ContainerOf(Memmy_Context_Get()->backend, Memmy_DarwinBackend, backend);
    Memmy_DarwinProcessData *data = Arena_PushStruct(arena, Memmy_DarwinProcessData);
    data->task = task;
    data->owns_task = owns_task;

    Memmy_Process *process = Arena_PushStruct(arena, Memmy_Process);
    process->backend = &backend->backend;
    process->pid = pid;
    process->pointer_width = Memmy_Darwin_PointerWidth();
    process->backend_data = data;
    *out = process;
    return Memmy_Status_Ok;
}

static void Memmy_Darwin_CloseProcess(Memmy_Process *process)
{
    Memmy_DarwinProcessData *data = (Memmy_DarwinProcessData *)process->backend_data;
    if (data != 0 && data->owns_task && data->task != MACH_PORT_NULL)
    {
        mach_port_deallocate(mach_task_self(), data->task);
        data->task = MACH_PORT_NULL;
    }
    process->backend_data = 0;
}

static Memmy_Status Memmy_Darwin_Read(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size, U64 *bytes_read,
                                      Memmy_Error *error)
{
    Memmy_DarwinProcessData *data = (Memmy_DarwinProcessData *)process->backend_data;
    *bytes_read = 0;

    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(data->task, (mach_vm_address_t)addr, (mach_vm_size_t)size,
                                              (mach_vm_address_t)(uintptr_t)buffer, &got);
    *bytes_read = (U64)got;
    if (kr != KERN_SUCCESS)
    {
        Memmy_Status status =
            got > 0 ? Memmy_Status_PartialRead : Memmy_Darwin_StatusFromKern(kr, Memmy_Status_Unreadable);
        Memmy_Darwin_SetError(error, status, String8_Lit("mach_vm_read_overwrite failed"), kr);
        if (error != 0)
        {
            error->byte_count = (U64)got;
        }
        return status;
    }
    if (got != (mach_vm_size_t)size)
    {
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("darwin"), String8_Lit("partial read"));
        if (error != 0)
        {
            error->byte_count = (U64)got;
        }
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_Write(Memmy_Process *process, Memmy_Addr addr, void const *buffer, U64 size,
                                       U64 *bytes_written, Memmy_Error *error)
{
    Memmy_DarwinProcessData *data = (Memmy_DarwinProcessData *)process->backend_data;
    *bytes_written = 0;

    if (size > (U64)UINT32_MAX)
    {
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("darwin"), String8_Lit("write size overflow"));
        return Memmy_Status_Overflow;
    }

    kern_return_t kr = mach_vm_write(data->task, (mach_vm_address_t)addr, (vm_offset_t)(uintptr_t)buffer,
                                     (mach_msg_type_number_t)size);
    if (kr != KERN_SUCCESS)
    {
        Memmy_Status status = Memmy_Darwin_StatusFromKern(kr, Memmy_Status_Unwritable);
        Memmy_Darwin_SetError(error, status, String8_Lit("mach_vm_write failed"), kr);
        return status;
    }

    *bytes_written = size;
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_ReadCString(Arena *arena, Memmy_Process *process, Memmy_Addr addr, U64 max_len,
                                             String8 *out, Memmy_Error *error)
{
    U8 *buffer = Arena_PushArrayNoZero(arena, U8, max_len + 1);
    U64 len = 0;
    for (; len < max_len; len++)
    {
        U64 bytes_read = 0;
        Memmy_Status status = Memmy_Darwin_Read(process, addr + len, buffer + len, 1, &bytes_read, error);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        if (buffer[len] == 0)
        {
            break;
        }
    }

    buffer[len] = 0;
    *out = String8_Make(buffer, len);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_ImageSize(Arena *arena, Memmy_Process *process, Memmy_Addr header_addr,
                                           Memmy_Size *out, Memmy_Error *error)
{
    *out = 0;

    struct mach_header_64 header = {0};
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Darwin_Read(process, header_addr, &header, sizeof(header), &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (header.magic != MH_MAGIC_64)
    {
        return Memmy_Status_Ok;
    }
    if (header.sizeofcmds == 0)
    {
        return Memmy_Status_Ok;
    }
    if (header.sizeofcmds > Megabytes(64))
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("darwin"),
                        String8_Lit("Mach-O load command table is too large"));
        return Memmy_Status_Unsupported;
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    U8 *commands = Arena_PushArrayNoZero(scratch.arena, U8, header.sizeofcmds);
    status = Memmy_Darwin_Read(process, header_addr + sizeof(header), commands, header.sizeofcmds, &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    U64 min_addr = U64_MAX;
    U64 max_addr = 0;
    U64 cursor_offset = 0;
    for (U32 i = 0; i < header.ncmds; i++)
    {
        if (cursor_offset + sizeof(struct load_command) > header.sizeofcmds)
        {
            break;
        }

        struct load_command const *load = (struct load_command const *)(commands + cursor_offset);
        if (load->cmdsize < sizeof(struct load_command) || cursor_offset + load->cmdsize > header.sizeofcmds)
        {
            break;
        }

        if (load->cmd == LC_SEGMENT_64)
        {
            if (load->cmdsize < sizeof(struct segment_command_64))
            {
                break;
            }
            struct segment_command_64 const *segment = (struct segment_command_64 const *)load;
            if (segment->vmsize != 0 && segment->initprot != VM_PROT_NONE)
            {
                U64 segment_end = 0;
                if (!AddU64Checked(segment->vmaddr, segment->vmsize, &segment_end))
                {
                    Scratch_End(scratch);
                    Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("darwin"),
                                    String8_Lit("Mach-O segment end overflow"));
                    return Memmy_Status_Overflow;
                }
                min_addr = Min(min_addr, segment->vmaddr);
                max_addr = Max(max_addr, segment_end);
            }
        }
        cursor_offset += load->cmdsize;
    }

    if (min_addr != U64_MAX && max_addr > min_addr)
    {
        *out = (Memmy_Size)(max_addr - min_addr);
    }

    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_ReadExact(Memmy_Process *process, Memmy_Addr addr, void *buffer, U64 size,
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
        Memmy_Error_Set(error, Memmy_Status_PartialRead, String8_Lit("darwin"),
                        String8_Lit("partial Mach-O metadata read"));
        if (error != 0)
        {
            error->byte_count = bytes_read;
        }
        return Memmy_Status_PartialRead;
    }
    return Memmy_Status_Ok;
}

static String8 Memmy_Darwin_Basename(String8 path)
{
    U64 start = 0;
    for (U64 i = 0; i < path.len; i++)
    {
        if (path.data[i] == '/')
        {
            start = i + 1;
        }
    }
    return String8_Make(path.data + start, path.len - start);
}

static Memmy_Status Memmy_Darwin_EnumerateModules(Arena *arena, Memmy_Process *process, Memmy_ModuleSink sink,
                                                  Memmy_Error *error)
{
    Memmy_DarwinProcessData *data = (Memmy_DarwinProcessData *)process->backend_data;
    task_dyld_info_data_t dyld_info = {0};
    mach_msg_type_number_t dyld_info_count = TASK_DYLD_INFO_COUNT;
    kern_return_t kr = task_info(data->task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &dyld_info_count);
    if (kr != KERN_SUCCESS)
    {
        Memmy_Status status = Memmy_Darwin_StatusFromKern(kr, Memmy_Status_PlatformError);
        Memmy_Darwin_SetError(error, status, String8_Lit("task_info TASK_DYLD_INFO failed"), kr);
        return status;
    }

    struct dyld_all_image_infos all_image_infos = {0};
    U64 bytes_read = 0;
    Memmy_Status status = Memmy_Darwin_Read(process, (Memmy_Addr)dyld_info.all_image_info_addr, &all_image_infos,
                                            sizeof(all_image_infos), &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (all_image_infos.infoArray == 0 || all_image_infos.infoArrayCount == 0)
    {
        return Memmy_Status_Ok;
    }
    if (all_image_infos.infoArrayCount > 65536)
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("darwin"),
                        String8_Lit("dyld image array is too large"));
        return Memmy_Status_Unsupported;
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    U64 image_infos_size = (U64)all_image_infos.infoArrayCount * sizeof(struct dyld_image_info);
    struct dyld_image_info *image_infos =
        Arena_PushArrayNoZero(scratch.arena, struct dyld_image_info, all_image_infos.infoArrayCount);
    status = Memmy_Darwin_Read(process, (Memmy_Addr)(uintptr_t)all_image_infos.infoArray, image_infos, image_infos_size,
                               &bytes_read, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    for (U32 i = 0; i < all_image_infos.infoArrayCount; i++)
    {
        Memmy_Addr base = (Memmy_Addr)(uintptr_t)image_infos[i].imageLoadAddress;
        if (base == 0)
        {
            continue;
        }

        Memmy_Size size = 0;
        status = Memmy_Darwin_ImageSize(arena, process, base, &size, error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }

        String8 path = {0};
        if (image_infos[i].imageFilePath != 0)
        {
            status = Memmy_Darwin_ReadCString(arena, process, (Memmy_Addr)(uintptr_t)image_infos[i].imageFilePath,
                                              PROC_PIDPATHINFO_MAXSIZE, &path, error);
            if (status != Memmy_Status_Ok)
            {
                Scratch_End(scratch);
                return status;
            }
        }

        Memmy_Module module = {
            .path = path,
            .name = String8_Copy(arena, Memmy_Darwin_Basename(path)),
            .base = base,
            .size = size,
        };
        status = sink.callback(sink.user_data, &module);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
    }

    Scratch_End(scratch);
    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_ModuleSearch_Push(void *user_data, Memmy_Module const *module)
{
    Memmy_DarwinModuleSearch *search = (Memmy_DarwinModuleSearch *)user_data;
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

static Memmy_Status Memmy_Darwin_FunctionMetadataNotFound(Memmy_Error *error)
{
    Memmy_Error_Set(error, Memmy_Status_NotFound, String8_Lit("function"), String8_Lit("function metadata not found"));
    return Memmy_Status_NotFound;
}

static B32 Memmy_Darwin_SegmentContainsFileRange(Memmy_DarwinSegmentInfo segment, U64 fileoff, U64 filesize)
{
    U64 segment_file_end = 0;
    U64 data_end = 0;
    if (!AddU64Checked(segment.fileoff, segment.filesize, &segment_file_end) ||
        !AddU64Checked(fileoff, filesize, &data_end))
    {
        return 0;
    }
    return fileoff >= segment.fileoff && data_end <= segment_file_end;
}

static Memmy_Status Memmy_Darwin_DecodeUleb128(U8 *data, U64 size, U64 *cursor, U64 *out, Memmy_Error *error)
{
    U64 result = 0;
    U32 shift = 0;
    while (*cursor < size)
    {
        U8 byte = data[*cursor];
        *cursor += 1;

        if (shift >= 64 || ((U64)(byte & 0x7f) << shift) >> shift != (U64)(byte & 0x7f))
        {
            return Memmy_Darwin_FunctionMetadataNotFound(error);
        }
        result |= (U64)(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0)
        {
            *out = result;
            return Memmy_Status_Ok;
        }
        shift += 7;
    }
    return Memmy_Darwin_FunctionMetadataNotFound(error);
}

static Memmy_Status Memmy_Darwin_FindFunction(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                              Memmy_Range *out, Memmy_Error *error)
{
    Memmy_DarwinModuleSearch search = {
        .address = address,
        .error = error,
    };
    Memmy_ModuleSink sink = {
        .callback = Memmy_Darwin_ModuleSearch_Push,
        .user_data = &search,
    };
    Memmy_Status status = Memmy_Darwin_EnumerateModules(arena, process, sink, error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (!search.found)
    {
        return Memmy_Darwin_FunctionMetadataNotFound(error);
    }

    Memmy_Module module = search.module;
    struct mach_header_64 header = {0};
    status = Memmy_Darwin_ReadExact(process, module.base, &header, sizeof(header), error);
    if (status != Memmy_Status_Ok)
    {
        return status;
    }
    if (header.magic != MH_MAGIC_64 || header.sizeofcmds == 0)
    {
        return Memmy_Darwin_FunctionMetadataNotFound(error);
    }
    if (header.sizeofcmds > Megabytes(64))
    {
        Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("darwin"),
                        String8_Lit("Mach-O load command table is too large"));
        return Memmy_Status_Unsupported;
    }

    Scratch scratch = Scratch_Begin(&arena, 1);
    U8 *commands = Arena_PushArrayNoZero(scratch.arena, U8, header.sizeofcmds);
    status = Memmy_Darwin_ReadExact(process, module.base + sizeof(header), commands, header.sizeofcmds, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    U64 function_starts_fileoff = 0;
    U64 function_starts_size = 0;
    B32 found_function_starts = 0;
    B32 found_image_base = 0;
    B32 found_function_starts_segment = 0;
    B32 found_text_end = 0;
    Memmy_DarwinSegmentInfo image_base_segment = {0};
    Memmy_DarwinSegmentInfo function_starts_segment = {0};
    U64 text_end_offset = 0;

    U64 cursor_offset = 0;
    for (U32 i = 0; i < header.ncmds; i++)
    {
        if (cursor_offset + sizeof(struct load_command) > header.sizeofcmds)
        {
            Scratch_End(scratch);
            return Memmy_Darwin_FunctionMetadataNotFound(error);
        }

        struct load_command const *load = (struct load_command const *)(commands + cursor_offset);
        if (load->cmdsize < sizeof(struct load_command) || cursor_offset + load->cmdsize > header.sizeofcmds)
        {
            Scratch_End(scratch);
            return Memmy_Darwin_FunctionMetadataNotFound(error);
        }

        if (load->cmd == LC_SEGMENT_64)
        {
            if (load->cmdsize < sizeof(struct segment_command_64))
            {
                Scratch_End(scratch);
                return Memmy_Darwin_FunctionMetadataNotFound(error);
            }

            struct segment_command_64 const *segment = (struct segment_command_64 const *)load;
            if (!found_image_base || segment->fileoff == 0)
            {
                image_base_segment = (Memmy_DarwinSegmentInfo){
                    .vmaddr = segment->vmaddr,
                    .vmsize = segment->vmsize,
                    .fileoff = segment->fileoff,
                    .filesize = segment->filesize,
                };
                found_image_base = 1;
            }

            if (found_function_starts && Memmy_Darwin_SegmentContainsFileRange(
                                             (Memmy_DarwinSegmentInfo){
                                                 .vmaddr = segment->vmaddr,
                                                 .vmsize = segment->vmsize,
                                                 .fileoff = segment->fileoff,
                                                 .filesize = segment->filesize,
                                             },
                                             function_starts_fileoff, function_starts_size))
            {
                function_starts_segment = (Memmy_DarwinSegmentInfo){
                    .vmaddr = segment->vmaddr,
                    .vmsize = segment->vmsize,
                    .fileoff = segment->fileoff,
                    .filesize = segment->filesize,
                };
                found_function_starts_segment = 1;
            }

            U64 section_table_size = (U64)segment->nsects * sizeof(struct section_64);
            if ((U64)load->cmdsize >= sizeof(struct segment_command_64) &&
                section_table_size <= (U64)load->cmdsize - sizeof(struct segment_command_64))
            {
                struct section_64 const *sections =
                    (struct section_64 const *)((U8 const *)segment + sizeof(struct segment_command_64));
                for (U32 section_index = 0; section_index < segment->nsects; section_index++)
                {
                    struct section_64 const *section = sections + section_index;
                    if (Memory_Equals(section->segname, "__TEXT", 6) && Memory_Equals(section->sectname, "__text", 6))
                    {
                        if (!AddU64Checked(section->addr, section->size, &text_end_offset))
                        {
                            Scratch_End(scratch);
                            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                                            String8_Lit("text section end overflow"));
                            return Memmy_Status_Overflow;
                        }
                        found_text_end = 1;
                    }
                }
            }
        }
        else if (load->cmd == LC_FUNCTION_STARTS)
        {
            if (load->cmdsize < sizeof(struct linkedit_data_command))
            {
                Scratch_End(scratch);
                return Memmy_Darwin_FunctionMetadataNotFound(error);
            }
            struct linkedit_data_command const *function_starts = (struct linkedit_data_command const *)load;
            function_starts_fileoff = function_starts->dataoff;
            function_starts_size = function_starts->datasize;
            found_function_starts = function_starts_size != 0;
        }

        cursor_offset += load->cmdsize;
    }

    if (!found_image_base || !found_function_starts)
    {
        Scratch_End(scratch);
        return Memmy_Darwin_FunctionMetadataNotFound(error);
    }

    if (!found_function_starts_segment)
    {
        cursor_offset = 0;
        for (U32 i = 0; i < header.ncmds; i++)
        {
            struct load_command const *load = (struct load_command const *)(commands + cursor_offset);
            if (load->cmd == LC_SEGMENT_64)
            {
                struct segment_command_64 const *segment = (struct segment_command_64 const *)load;
                Memmy_DarwinSegmentInfo candidate = {
                    .vmaddr = segment->vmaddr,
                    .vmsize = segment->vmsize,
                    .fileoff = segment->fileoff,
                    .filesize = segment->filesize,
                };
                if (Memmy_Darwin_SegmentContainsFileRange(candidate, function_starts_fileoff, function_starts_size))
                {
                    function_starts_segment = candidate;
                    found_function_starts_segment = 1;
                    break;
                }
            }
            cursor_offset += load->cmdsize;
        }
    }

    if (!found_function_starts_segment || function_starts_size > Megabytes(64))
    {
        Scratch_End(scratch);
        if (function_starts_size > Megabytes(64))
        {
            Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("darwin"),
                            String8_Lit("Mach-O function-start table is too large"));
            return Memmy_Status_Unsupported;
        }
        return Memmy_Darwin_FunctionMetadataNotFound(error);
    }

    U64 slide = 0;
    if (!SubU64Checked(module.base, image_base_segment.vmaddr, &slide))
    {
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("Mach-O slide overflow"));
        return Memmy_Status_Overflow;
    }

    U64 function_starts_segment_offset = function_starts_fileoff - function_starts_segment.fileoff;
    Memmy_Addr function_starts_addr = 0;
    if (!AddU64Checked(slide, function_starts_segment.vmaddr, &function_starts_addr) ||
        !AddU64Checked(function_starts_addr, function_starts_segment_offset, &function_starts_addr))
    {
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                        String8_Lit("function-start table address overflow"));
        return Memmy_Status_Overflow;
    }

    U8 *function_starts_data = Arena_PushArrayNoZero(scratch.arena, U8, function_starts_size);
    status = Memmy_Darwin_ReadExact(process, function_starts_addr, function_starts_data, function_starts_size, error);
    if (status != Memmy_Status_Ok)
    {
        Scratch_End(scratch);
        return status;
    }

    Memmy_Addr range_end = 0;
    if (found_text_end)
    {
        if (!AddU64Checked(slide, text_end_offset, &range_end))
        {
            Scratch_End(scratch);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                            String8_Lit("text section address overflow"));
            return Memmy_Status_Overflow;
        }
    }
    else if (!AddU64Checked(module.base, module.size, &range_end))
    {
        Scratch_End(scratch);
        Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"), String8_Lit("module end overflow"));
        return Memmy_Status_Overflow;
    }

    U64 function_offset = 0;
    Memmy_Addr previous_start = 0;
    B32 have_previous_start = 0;
    U64 function_starts_cursor = 0;
    while (function_starts_cursor < function_starts_size)
    {
        U64 delta = 0;
        status = Memmy_Darwin_DecodeUleb128(function_starts_data, function_starts_size, &function_starts_cursor, &delta,
                                            error);
        if (status != Memmy_Status_Ok)
        {
            Scratch_End(scratch);
            return status;
        }
        if (delta == 0)
        {
            break;
        }
        if (!AddU64Checked(function_offset, delta, &function_offset))
        {
            Scratch_End(scratch);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                            String8_Lit("function offset overflow"));
            return Memmy_Status_Overflow;
        }

        Memmy_Addr start = 0;
        if (!AddU64Checked(module.base, function_offset, &start))
        {
            Scratch_End(scratch);
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("function"),
                            String8_Lit("function address overflow"));
            return Memmy_Status_Overflow;
        }

        if (have_previous_start && address >= previous_start && address < start)
        {
            status = Memmy_Range_FromStartEnd(previous_start, start, out, error);
            Scratch_End(scratch);
            return status;
        }

        previous_start = start;
        have_previous_start = 1;
    }

    if (have_previous_start && address >= previous_start && address < range_end)
    {
        status = Memmy_Range_FromStartEnd(previous_start, range_end, out, error);
        Scratch_End(scratch);
        return status;
    }

    Scratch_End(scratch);
    return Memmy_Darwin_FunctionMetadataNotFound(error);
}

static Memmy_RegionAccess Memmy_Darwin_RegionAccess(vm_prot_t protection)
{
    Memmy_RegionAccess result = 0;
    if (protection & VM_PROT_READ)
    {
        result |= Memmy_RegionAccess_Read;
    }
    if (protection & VM_PROT_WRITE)
    {
        result |= Memmy_RegionAccess_Write;
    }
    if (protection & VM_PROT_EXECUTE)
    {
        result |= Memmy_RegionAccess_Execute;
    }
    return result;
}

static Memmy_Status Memmy_Darwin_EnumerateRegions(Arena *arena, Memmy_Process *process, Memmy_RegionSink sink,
                                                  Memmy_Error *error)
{
    Unused(arena);

    Memmy_DarwinProcessData *data = (Memmy_DarwinProcessData *)process->backend_data;
    mach_vm_address_t address = 0;
    natural_t depth = 0;
    for (;;)
    {
        mach_vm_size_t size = 0;
        vm_region_submap_info_data_64_t info = {0};
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr =
            mach_vm_region_recurse(data->task, &address, &size, &depth, (vm_region_recurse_info_t)&info, &count);
        if (kr == KERN_INVALID_ADDRESS)
        {
            break;
        }
        if (kr != KERN_SUCCESS)
        {
            Memmy_Status status = Memmy_Darwin_StatusFromKern(kr, Memmy_Status_PlatformError);
            Memmy_Darwin_SetError(error, status, String8_Lit("mach_vm_region_recurse failed"), kr);
            return status;
        }
        if (info.is_submap)
        {
            depth++;
            continue;
        }

        Memmy_Region region = {
            .base = (Memmy_Addr)address,
            .size = (Memmy_Size)size,
            .access = Memmy_Darwin_RegionAccess(info.protection),
            .state = Memmy_RegionState_Committed,
        };

        U64 next = 0;
        if (!AddU64Checked(region.base, region.size, &next) || next <= region.base)
        {
            Memmy_Error_Set(error, Memmy_Status_Overflow, String8_Lit("darwin"), String8_Lit("region end overflow"));
            return Memmy_Status_Overflow;
        }
        Memmy_Status status = sink.callback(sink.user_data, &region);
        if (status != Memmy_Status_Ok)
        {
            return status;
        }
        address = (mach_vm_address_t)next;
    }

    return Memmy_Status_Ok;
}

static Memmy_Status Memmy_Darwin_FindObjectBase(Arena *arena, Memmy_Process *process, Memmy_Addr address,
                                                Memmy_ObjectBaseOptions const *options, Memmy_ObjectBaseResult *out,
                                                Memmy_Error *error)
{
    Unused(arena);
    Unused(process);
    Unused(address);
    Unused(options);
    Unused(out);

    Memmy_Error_Set(error, Memmy_Status_Unsupported, String8_Lit("objectbase"),
                    String8_Lit("Darwin object-base discovery is not implemented"));
    return Memmy_Status_Unsupported;
}

Memmy_Backend *Memmy_DarwinBackend_Create(Arena *arena)
{
    Memmy_DarwinBackend *backend = Arena_PushStruct(arena, Memmy_DarwinBackend);
    backend->backend = (Memmy_Backend){
        .name = String8_Lit("darwin"),
        .enumerate_processes = Memmy_Darwin_EnumerateProcesses,
        .open_process = Memmy_Darwin_OpenProcess,
        .close_process = Memmy_Darwin_CloseProcess,
        .read = Memmy_Darwin_Read,
        .write = Memmy_Darwin_Write,
        .enumerate_modules = Memmy_Darwin_EnumerateModules,
        .enumerate_regions = Memmy_Darwin_EnumerateRegions,
        .find_function = Memmy_Darwin_FindFunction,
        .find_object_base = Memmy_Darwin_FindObjectBase,
    };
    return &backend->backend;
}

#endif
