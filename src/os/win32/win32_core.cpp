// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include <ShObjIdl.h>
#include <wchar.h>

#include "base/base_core.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "base/array.h"
#include "base/string.h"

U64 os_now_microseconds(void)
{
    U64 result = 0;
    LARGE_INTEGER large_int_counter;
    if (QueryPerformanceCounter(&large_int_counter))
    {
        result = (large_int_counter.QuadPart * Mil(1)) / os_info.microsecond_resolution;
    }
    return result;
}

global_v U32 win32_sleep_ms_from_us(U64 end_us)
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

// Reference: https://stackoverflow.com/a/46024468
global_v const U64 UNIX_TIME_START = 0x019DB1DED53E8000; //January 1, 1970 (start of Unix epoch) in "ticks"
global_v const U64 TICKS_PER_SECOND = 10000000;          //a tick is 100ns

void win32_to_unix_timestamp(U64 *win_timestamp)
{
    *win_timestamp -= UNIX_TIME_START;
    *win_timestamp /= TICKS_PER_SECOND;
}

void win32_format_path(StringBuilder *dir)
{
    string_replace(dir, "\\", "/");
}

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
    os_info.worker_count = sysinfo.dwNumberOfProcessors;

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

String os_env_var(const char *name, Arena *arena)
{
    char buffer[KB(4)];

    U64 size = GetEnvironmentVariable(name, buffer, KB(4));
    return string_copy(arena, buffer, size);
}

void os_mkdirs(String path)
{
    char buffer[KB(4)] = {0};
    MemoryCopy(buffer, path.v, path.size);

    for (U64 i = 0; i < path.size; i++)
    {
        if (buffer[i] == '\\' || buffer[i] == '/')
        {
            char cur = buffer[i];
            buffer[i] = 0;

            BOOL success = CreateDirectory(buffer, NULL);
            DWORD err = GetLastError();

            Assert(success || err == ERROR_ALREADY_EXISTS, "Failed to create directory (%s)", buffer);
            buffer[i] = cur;
        }
    }

    BOOL success = CreateDirectory(buffer, NULL);
    DWORD err = GetLastError();

    Assert(success || err == ERROR_ALREADY_EXISTS, "Failed to create directory (%s)", buffer);
}

U64 os_get_timestamp()
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    U64 time = ft.dwHighDateTime;
    time <<= 32;
    time |= ft.dwLowDateTime;

    win32_to_unix_timestamp(&time);

    return time;
}

Time os_get_localtime()
{
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    return {
        .date = local_time.wDay,
        .month = (Month)(local_time.wMonth - 1),
        .year = local_time.wYear,

        .hour = local_time.wHour,
        .minute = local_time.wMinute,
        .second = local_time.wSecond,
        .milsec = local_time.wMilliseconds,
    };
}

U64 os_get_ticks_now()
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}

U64 os_get_ticks_freq()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

// NOTE: windows locks a .dll file once it's opened using loadlibrary, so every
// re-build creates a new file with a random number (pagesXXX.dll) and the file
// that is modified last is picked and loaded. This is not required on UNIX
LibHandle os_loadlib(const char *filename)
{
    char buffer[KB(4)] = {0};
    WIN32_FIND_DATA fdFile = {0};
    HANDLE hFind = INVALID_HANDLE_VALUE;

    const char *postfix = "*.dll";
    String filename_str = sv(filename);
    MemoryCopy(buffer, filename, filename_str.size);
    MemoryCopy(buffer + filename_str.size, postfix, sizeof(postfix) + 1);
    String exp = {.v = (U8 *)buffer, .size = filename_str.size + sizeof(postfix) + 1};

    U64 mtime = 0;
    String libfile = {0};
    hFind = FindFirstFile(CStrCast(exp), &fdFile);

    do
    {
        if (hFind == INVALID_HANDLE_VALUE)
            break;

        String cur_file = sv(fdFile.cFileName);

        if (match_front(cur_file, ".") || match_front(cur_file, ".."))
            continue;

        if ((fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            U64 file_mtime = fdFile.ftLastWriteTime.dwHighDateTime;
            file_mtime <<= 32;
            file_mtime |= fdFile.ftLastWriteTime.dwLowDateTime;

            if (file_mtime > mtime)
            {
                mtime = file_mtime;
                MemoryCopy(buffer, CStrCast(cur_file), cur_file.size + 1);
                libfile = {.v = (U8 *)buffer, .size = filename_str.size};
            }
        }

    } while (FindNextFile(hFind, &fdFile) != 0);
    FindClose(hFind);

    Assert(libfile.size, "libfile not found by name");

    HMODULE lib = LoadLibrary(CStrCast(libfile));
    Assert(lib, "lib is invalid");
    return lib;
}

WString os_select_dir(const wchar *title, const wchar *default_path, Arena *arena)
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

    WString res = string_copy(arena, path);

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
        _Win32Trap(res, (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND),
                   // NOTE: otherwise it's a real error
                   err);
        return false;
    }
    return true;
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

Semaphore os_semaphore_init(S32 initial, S32 max)
{
    HANDLE h = CreateSemaphore(0, initial, max, NULL);
    if (!h)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
    return h;
}

void os_semaphore_destroy(Semaphore s)
{
    B32 result = (CloseHandle(s) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

void os_semaphore_push(Semaphore s)
{
    B32 result = (ReleaseSemaphore(s, 1, 0) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        Assert(0, "Error Code: 0x%X (%u)\n", err, err);
    }
}

B32 os_semaphore_pop(Semaphore s, U64 end_us)
{
    U32 sleep = win32_sleep_ms_from_us(end_us);
    DWORD wait_result = WaitForSingleObject(s, sleep);
    B32 result = (wait_result == WAIT_OBJECT_0);
    return result;
}

void os_mutex_init(Mutex *mutex)
{
    InitializeCriticalSection(mutex);
}

void os_mutex_destroy(Mutex *mutex)
{
    DeleteCriticalSection(mutex);
}

void os_mutex_lock(Mutex *mutex)
{
    EnterCriticalSection(mutex);
}

void os_mutex_unlock(Mutex *mutex)
{
    LeaveCriticalSection(mutex);
}

B32 os_mutex_trylock(Mutex *mutex)
{
    return (TryEnterCriticalSection(mutex) == 1) ? (1) : (0);
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

void os_thread_join(Thread t)
{
    if (t != 0)
    {
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
}

FileHandle _os_file_open(String path, FileAccess access, FileMode mode, Result *res, U64 size)
{
    ClearResult(res);

    DWORD desired_access = 0;
    if (access & FileAccess_Read)
        desired_access |= GENERIC_READ;
    if (access & FileAccess_Write)
        desired_access |= GENERIC_WRITE;
    if (access & FileAccess_Append)
        desired_access |= FILE_APPEND_DATA;
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

void os_file_delete(String path, Result *res)
{
    BOOL out = DeleteFile(CStrCast(path));
    Win32Trap(res, out);
}

void os_file_close(FileHandle file_desc, Result *res)
{
    ClearResult(res);
    if (file_desc == INVALID_HANDLE_VALUE || file_desc == 0)
        return;
    BOOL out = CloseHandle(file_desc);
    Win32Trap(res, out);
}

U64 _os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
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

U32 _os_file_read(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
{
    ClearResult(res);
    LARGE_INTEGER off_large_int = {0};
    off_large_int.QuadPart = offset;

    OVERLAPPED off_overlap = {0};
    off_overlap.Offset = off_large_int.LowPart;
    off_overlap.OffsetHigh = off_large_int.HighPart;

    DWORD read = 0;

    BOOL out = ReadFile(file_desc, buffer, size, &read, &off_overlap);
    Win32Trap(res, out);
    return read;
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

FileMTime *os_list_by_pattern(String pattern, String base, Arena *arena)
{
    FileMTime *res = NULL;

    WIN32_FIND_DATA fdFile = {0};
    HANDLE hFind = INVALID_HANDLE_VALUE;

    hFind = FindFirstFile(CStrCast(pattern), &fdFile);

    do
    {
        if (hFind == INVALID_HANDLE_VALUE)
            break;

        String filename = sv(fdFile.cFileName);

        if (match_front(filename, ".") || match_front(filename, ".."))
            continue;

        // mscbl_log_info("Logfile: %.*ls", WStringSpr(filename));

        if ((fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            FILETIME write_time = fdFile.ftLastWriteTime;
            U64 mtime = 0;
            mtime = write_time.dwHighDateTime;
            mtime <<= 32;
            mtime |= write_time.dwLowDateTime;

            win32_to_unix_timestamp(&mtime);

            StringBuilder file_path = string_init(arena, base);
            path_join(&file_path, filename);

            FileMTime entry = {
                .path = StringCast(file_path),
                .mtime = mtime};
            da_push(arena, res, entry);
        }
    } while (FindNextFile(hFind, &fdFile));
    FindClose(hFind);

    return res;
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
        CloseHandle(handle);
        handle = NULL;
    }
Return:
    return {.data = map,
            .size = size,
            .handle = handle};
}

void os_file_unmap(OSMmap map, Result *res)
{
    ClearResult(res);
    if (map.handle == INVALID_HANDLE_VALUE || map.handle == 0)
        return;
    FlushViewOfFile(map.data, map.size);
    UnmapViewOfFile(map.data);
    BOOL out = CloseHandle(map.handle);
    Win32Trap(res, out);
}
