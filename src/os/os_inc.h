#pragma once
#include "base/base_core.h"
#include "base/threads.h"

#if OS_WINDOWS
#include "os/win32/os_core_win32.h"
#elif OS_LINUX
#include "os/linux/os_core_linux.h"
#endif

global_v OSInfo os_info = {0};

Guid os_make_guid();
void os_prelaunch();

void os_loadlib(const char *filename, const char *func_name, void *func);

void *os_reserve(void *ptr, U64 size);
void os_release(void *ptr, U64 size);
void os_commit(void *ptr, U64 size);
void os_decommit(void *ptr, U64 size);

Semaphore os_semaphore_alloc(U32 initial, U32 max);
void os_semaphore_release(Semaphore s);
void os_semaphore_drop(Semaphore s);
B32 os_semaphore_take(Semaphore s, U64 end_us);

void os_thread_detach(Thread t);
