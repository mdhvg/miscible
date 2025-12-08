#pragma once

#include <windows.h>

#include "base/core.h"
#include "base/threadpool.h"

#define OS_SEMAPHORE_MAX		LONG_MAX
#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T		LPTHREAD_START_ROUTINE

struct OSInfo
{
	Arena *arena;
	U64 microsecond_resolution;
};

global OSInfo os_info = {0};

Guid os_make_guid();
void os_prelaunch();

Semaphore os_semaphore_alloc(U32 initial, U32 max);
void os_semaphore_release(Semaphore s);
void os_semaphore_drop(Semaphore s);
B8 os_semaphore_take(Semaphore s, U64 end_us);

Thread os_thread_launch(LPTHREAD_START_ROUTINE fn, Worker *worker);
void os_thread_detach(Thread t);