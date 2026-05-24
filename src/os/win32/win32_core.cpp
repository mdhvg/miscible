#include <ShObjIdl.h>
#include <wchar.h>

#include "base/string.h"
#include "os/os_inc.h"
#include "base/log.h"
#include "base/base_core.h"

Guid os_make_guid()
{
    Guid result;
    MemoryZeroStruct(&result);
    UUID uuid;
    RPC_STATUS rpc_status = UuidCreate(&uuid);
    if (rpc_status == RPC_S_OK)
    {
        result.data1 = uuid.Data1;
        result.data2 = uuid.Data2;
        result.data3 = uuid.Data3;
        MemoryCopyArray(result.data4, uuid.Data4);
    }
    return result;
}

void os_prelaunch()
{
    os_info.microsecond_resolution = 1;
    LARGE_INTEGER large_int_resolution;
    if (QueryPerformanceFrequency(&large_int_resolution))
    {
        os_info.microsecond_resolution = large_int_resolution.QuadPart;
    }

    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    os_info.page_size = sysinfo.dwPageSize;

    // Set terminal UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // For <shobjidl.h>
    CoInitialize(NULL);
}

void os_cleanup()
{
    CoUninitialize();
}

const char *os_gethome()
{
    return getenv("USERPROFILE");
}

void os_mkdir(String path)
{
    Assert(CreateDirectoryA(CStrCast(path), NULL) || GetLastError() == ERROR_ALREADY_EXISTS, "Failed to create directory (%.*s)", path.size, path.v);
}

void os_loadlib(const char *filename, const char *func_name, void *func)
{
    LoadLibraryA(filename);
}

local_v U64 os_now_microseconds(void)
{
    U64 result = 0;
    LARGE_INTEGER large_int_counter;
    if (QueryPerformanceCounter(&large_int_counter))
    {
        result = (large_int_counter.QuadPart * Mil(1)) / os_info.microsecond_resolution;
    }
    return result;
}

local_v U32 win32_sleep_ms_from_us(U64 end_us)
{
    if (end_us == U64_MAX)
        return INFINITE;

    U32 sleep_ms = 0;
    U64 begint = os_now_microseconds();
    if (begint < end_us)
    {
        U64 sleep_us = end_us - begint;
        sleep_ms = (U32)((sleep_us + 999) / 1000);
    }
    return sleep_ms;
}

OSString os_select_dir(const wchar *title, const wchar *default_path)
{
    PWSTR path = NULL;
    IShellItem *res_psi = NULL;
    IShellItem *def_psi = NULL;
    IFileOpenDialog *pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void **)&pfd);

    Assert(SUCCEEDED(hr), "CoCreateInstance failed");
    pfd->SetOptions(FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    pfd->SetTitle(title);

    if (default_path && *default_path && SUCCEEDED(SHCreateItemFromParsingName(default_path, NULL, IID_IShellItem, (void **)&def_psi)))
    {
        pfd->SetFolder(def_psi);
        pfd->SetDefaultFolder(def_psi);
        def_psi->Release();
    }

    if (FAILED(pfd->Show(NULL)))
        return {0};
    Assert(SUCCEEDED(pfd->GetResult(&res_psi)), "selection box GetResult() failed");
    res_psi->GetDisplayName(SIGDN_FILESYSPATH, &path);

    MemoryCopy(scratch, path, (wcslen(path) + 1) * sizeof(wchar));
    WString res = sv(scratch);

    CoTaskMemFree(path);
    res_psi->Release();
    pfd->Release();
    return res;
}

#define _Win32Trap(res, cnd, err)     \
    do                                \
    {                                 \
        if (!cnd && res)              \
        {                             \
            *(res) = {                \
                .success = 0,         \
                .domain = Domain_OS,  \
                .code = (err),        \
                .context = __func__}; \
        }                             \
    } while (0)
#define Win32Trap(res, cnd) _Win32Trap(res, cnd, GetLastError())

B32 os_path_exists(String path, Result *res)
{
    ClearResult(res);
    DWORD attrs = GetFileAttributesA(CStrCast(path));

    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        U32 err = GetLastError();
        _Win32Trap(res, (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND),
                   // NOTE: otherwise it's a real error
                   err);
    }
    return true;
}

void win32_format_path(StringBuilder *dir)
{
    string_replace(dir, "\\", "/");
}

void *os_reserve(void *ptr, U64 size)
{
    if (!size) return 0;
    void *result = VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);

    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
    return result;
}

void os_release(void *ptr, U64 size)
{
    B32 result = (VirtualFree(ptr, 0, MEM_RELEASE) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

void os_commit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

void os_decommit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (VirtualFree(ptr, size, MEM_DECOMMIT) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

Semaphore os_semaphore_alloc(S32 initial, S32 max)
{
    HANDLE h = CreateSemaphore(0, initial, max, NULL);
    if (!h)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
    return h;
}

void os_semaphore_release(Semaphore s)
{
    B32 result = (CloseHandle(s) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

void os_semaphore_drop(Semaphore s)
{
    B32 result = (ReleaseSemaphore(s, 1, 0) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

B32 os_semaphore_take(Semaphore s, U64 end_us)
{
    U32 sleep = win32_sleep_ms_from_us(end_us);
    DWORD wait_result = WaitForSingleObject(s, sleep);
    B32 result = (wait_result == WAIT_OBJECT_0);
    return result;
}

Thread os_thread_launch(LPTHREAD_START_ROUTINE fn, Worker *worker)
{
    return CreateThread(0, 0, fn, worker, 0, NULL);
}

void os_thread_detach(Thread t)
{
    if (t != 0)
    {
        CloseHandle(t);
    }
}

FileHandle os_file_open(String path, FileAccess access, FileMode mode, Result *res, U64 size)
{
    ClearResult(res);

    DWORD desired_access = 0;
    if (access & FileAccess_Read)
        desired_access |= GENERIC_READ;
    if (access & FileAccess_Write)
        desired_access |= GENERIC_WRITE;
    Assert(desired_access, "needs to have read/write/both access");

    DWORD creation_mode = 0;
    switch (mode)
    {
    case FileMode_CreateAlways:
        creation_mode = CREATE_ALWAYS;
        break;
    case FileMode_OpenAlways:
        creation_mode = OPEN_ALWAYS;
        break;
    default:
        Assert(0, "invalid file open mode %lu", mode);
    }

    HANDLE handle = CreateFileA(CStrCast(path), desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, creation_mode, FILE_ATTRIBUTE_NORMAL, NULL);

    Win32Trap(res, (handle != INVALID_HANDLE_VALUE));
    return handle;
}

void os_file_close(FileHandle file_desc, Result *res)
{
    ClearResult(res);
    if (file_desc == INVALID_HANDLE_VALUE || file_desc == 0)
        return;
    BOOL out = CloseHandle(file_desc);
    Win32Trap(res, out);
}

U64 os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
{
    ClearResult(res);
    LARGE_INTEGER off_large_int = {0};
    off_large_int.QuadPart = offset;

    OVERLAPPED off_overlap = {0};
    off_overlap.Offset = off_large_int.LowPart;
    off_overlap.OffsetHigh = off_large_int.HighPart;

    DWORD written = 0;

    BOOL out = WriteFile(file_desc, buffer, size, &written, &off_overlap);
    Win32Trap(res, out);
    return written;
}

U32 os_file_read(FileHandle file_desc, U64 size, U8 *buffer, Result *res)
{
    ClearResult(res);
    DWORD bytes_read = 0;
    BOOL out = ReadFile(file_desc, buffer, size, &bytes_read, NULL);
    Win32Trap(res, out);
    return bytes_read;
}

U64 os_file_size(FileHandle file_desc, Result *res)
{
    ClearResult(res);
    LARGE_INTEGER size = {0};
    BOOL out = GetFileSizeEx(file_desc, &size);
    Win32Trap(res, out);
    return size.QuadPart;
}

void os_file_rename(String old_path, String new_path)
{
    DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED;
    Assert(MoveFileEx(CStrCast(old_path), CStrCast(new_path), flags), "error code: %lu", GetLastError());
}

OSMmap os_file_map(FileHandle file_desc, U64 size, Result *res)
{
    ClearResult(res);
    void *map = NULL;
    // NOTE: store size as size_high:size_low format
    DWORD size_high = (DWORD)((size >> 32) & 0xFFFFFFFF);
    DWORD size_low = (DWORD)(size & 0xFFFFFFFF);

    HANDLE handle = CreateFileMappingA(file_desc, NULL, PAGE_READWRITE, size_high, size_low, NULL);
    Win32Trap(res, handle);
    if (!res->success)
        goto Return;

    map = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
    Win32Trap(res, map);
    if (!res->success)
        goto Cleanup;

    goto Return;

Cleanup:
    if (handle)
    {
        BOOL out = CloseHandle(handle);
        handle = NULL;
    }
Return:
    return {.data = map,
            .size = size,
            .map_handle = handle};
}

void os_file_unmap(OSMmap map)
{
    FlushViewOfFile(map.data, map.size);
    UnmapViewOfFile(map.data);
    CloseHandle(map.map_handle);
}
