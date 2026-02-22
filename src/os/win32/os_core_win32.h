#pragma once
#include "base/string.h"
#define NOMINMAX
#include <Windows.h>

#include "base/base_core.h"

#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T     LPTHREAD_START_ROUTINE

#define OSchar    wchar
#define W(x)      Glue(L, x)
#define Semaphore HANDLE
#define Thread    HANDLE

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

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, struct Worker *worker);
