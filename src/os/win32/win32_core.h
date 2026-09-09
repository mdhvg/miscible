// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#define NOMINMAX
#include <Windows.h>
#include <dbghelp.h>

#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T     LPTHREAD_START_ROUTINE

#define OSSlash "\\"
#define LibExt  ".dll"

typedef wchar_t OSChar;
typedef HANDLE FileHandle;

typedef HMODULE LibHandle;
typedef FARPROC LibAddress;

typedef HANDLE Process;
typedef HANDLE Thread;
typedef HANDLE Semaphore;
typedef CRITICAL_SECTION Mutex;

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "normaliz.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "dbghelp.lib")

struct OSMmap
{
    void *data;
    U64 size;
    HANDLE handle;
};

struct OSInfo
{
    Time start_time;
    U64 start_ticks;
    U64 ticks_per_sec;

    U64 nproc;
    U32 proc_id;
    Process process;

    U64 page_size;
    Arena *arena;
};

inline void win32_sleep_ms(U64 ms)
{
    Sleep(ms);
}

MSCBL_API void win32_format_path(StringBuilder *dir);
void win32_to_unix_timestamp(U64 *win_timestamp);

inline LibAddress os_libfunc(LibHandle lib, const char *symbol)
{
    return GetProcAddress(lib, symbol);
}
inline void os_closelib(LibHandle lib)
{
    FreeLibrary(lib);
}

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, struct Worker *worker);
