#pragma once
#define NOMINMAX
#include <Windows.h>

#include "base/string.h"

#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T     LPTHREAD_START_ROUTINE

#define OSchar     wchar
#define W(x)       Glue(L, x)
#define Semaphore  HANDLE
#define Mutex      CRITICAL_SECTION
#define Thread     HANDLE
#define LibHandle  HMODULE
#define LibAddress FARPROC
#define LibExt     ".dll"

typedef HANDLE FileHandle;

struct OSMmap
{
    void *data;
    U64 size;
    HANDLE handle;
};

struct OSInfo
{
    OS_COMMON;
    U64 microsecond_resolution;
};

inline void win32_sleep_ms(U64 ms)
{
    Sleep(ms);
}

MSCBL_API void win32_format_path(StringBuilder *dir);

inline LibHandle os_loadlib(const char *filename)
{
    return LoadLibrary(filename);
}
inline LibAddress os_libfunc(LibHandle lib, const char *symbol)
{
    return GetProcAddress(lib, symbol);
}
inline void os_closelib(LibHandle lib)
{
    FreeLibrary(lib);
}

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, struct Worker *worker);
