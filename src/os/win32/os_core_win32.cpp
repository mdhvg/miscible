#include <ShObjIdl.h>

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
    Assert(CreateDirectoryA(CStrCast(path), NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
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
    U64 begint   = os_now_microseconds();
    if (begint < end_us)
    {
        U64 sleep_us = end_us - begint;
        sleep_ms     = (U32)((sleep_us + 999) / 1000);
    }
    return sleep_ms;
}

void os_select_dir(const wchar *title, const wchar *default_path, StringBuilder *sb)
{
    PWSTR path           = NULL;
    IShellItem *res_psi  = NULL;
    IShellItem *def_psi  = NULL;
    IFileOpenDialog *pfd = NULL;
    HRESULT hr           = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void **)&pfd);

    Assert(SUCCEEDED(hr) && "CoCreateInstance failed");
    pfd->SetOptions(FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    pfd->SetTitle(title);

    if (default_path && *default_path && SUCCEEDED(SHCreateItemFromParsingName(default_path, NULL, IID_IShellItem, (void **)&def_psi)))
    {
        pfd->SetFolder(def_psi);
        pfd->SetDefaultFolder(def_psi);
        def_psi->Release();
    }

    Assert(SUCCEEDED(pfd->Show(NULL)));
    Assert(SUCCEEDED(pfd->GetResult(&res_psi)));
    res_psi->GetDisplayName(SIGDN_FILESYSPATH, &path);

    string_push(sb, path);

    CoTaskMemFree(path);
    res_psi->Release();
    pfd->Release();
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
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
    return result;
}

void os_release(void *ptr, U64 size)
{
    B32 result = (VirtualFree(ptr, 0, MEM_RELEASE) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
}

void os_commit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
}

void os_decommit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (VirtualFree(ptr, size, MEM_DECOMMIT) != 0);

    if (!result)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
}

Semaphore os_semaphore_alloc(U32 initial, U32 max)
{
    HANDLE h = CreateSemaphore(0, initial, max, NULL);
    if (!h)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
    return h;
}

void os_semaphore_release(Semaphore s)
{
    B32 result = (CloseHandle(s) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
}

void os_semaphore_drop(Semaphore s)
{
    B32 result = (ReleaseSemaphore(s, 1, 0) != 0);
    if (!result)
    {
        U32 err = GetLastError();
        mscbl_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
        Assert(0);
    }
}

B32 os_semaphore_take(Semaphore s, U64 end_us)
{
    U32 sleep         = win32_sleep_ms_from_us(end_us);
    DWORD wait_result = WaitForSingleObject(s, sleep);
    B32 result        = (wait_result == WAIT_OBJECT_0);
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
