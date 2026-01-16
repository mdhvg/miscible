#pragma once
#define NOMINMAX
#include <windows.h>

#include "base/base_core.h"
#include "base/threadpool.h"

#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T     LPTHREAD_START_ROUTINE

struct OSInfo
{
    OS_COMMON;
    U64 microsecond_resolution;
};

inline void win32_sleep_ms(U64 ms)
{
    Sleep(ms);
}

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, Worker *worker);
