#pragma once
#include "base/base_core.h"
#include "base/string.h"

#define OS_COMMON                \
    struct                       \
    {                            \
        struct ThreadPool *pool; \
        U64 worker_count;        \
        U64 page_size;           \
    }

union Guid {
    struct
    {
        U32 data1;
        U16 data2;
        U16 data3;
        U8 data4[8];
    };
    U8 v[16];
};

#if OS_WINDOWS
#include "os/win32/os_core_win32.h"
#elif OS_LINUX
#include "os/linux/os_core_linux.h"
#endif

#ifndef OSchar
#error "OSchar not defined"
#endif
#ifndef W
#error "OS char converter not defined"
#endif
#ifndef Semaphore
#error "Semaphore not defined"
#endif
#ifndef Thread
#error "Thread not defined"
#endif

MSCBL_API OSInfo os_info;

Guid os_make_guid();
void os_prelaunch();
void os_cleanup();
const char *os_gethome();
void os_mkdir(String path);

void os_loadlib(const char *filename, const char *func_name, void *func);
MSCBL_API void os_select_dir(const OSchar *title, const wchar *default_path, StringBuilder *sb);

void *os_reserve(void *ptr, U64 size);
void os_release(void *ptr, U64 size);
void os_commit(void *ptr, U64 size);
void os_decommit(void *ptr, U64 size);

Semaphore os_semaphore_alloc(U32 initial, U32 max);
void os_semaphore_release(Semaphore s);
void os_semaphore_drop(Semaphore s);
B32 os_semaphore_take(Semaphore s, U64 end_us);

void os_thread_detach(Thread t);
