#pragma once
#include "base/string.h"

// NOTE: for an os layer to be compatible with this application, it
// needs implementation for all of this...

#define OS_COMMON         \
    struct                \
    {                     \
        U64 worker_count; \
        U64 page_size;    \
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
#include "os/win32/win32_core.h"
#elif OS_LINUX
#include "os/linux/linux_core.h"
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
#ifndef Mutex
#error "Mutex not defined"
#endif
#ifndef Thread
#error "Thread not defined"
#endif
#if !defined(LibHandle) || !defined(LibAddress)
#error "dll types not defined"
#endif
#ifndef LibExt
#error "LibExt not defined"
#endif

MSCBL_API OSInfo os_info;
typedef struct OSMmap OSMmap;

Guid os_make_guid();
void os_prelaunch();
void os_cleanup();
const char *os_gethome();
void os_mkdir(String path);

LibHandle os_loadlib(const char *filename);
LibAddress os_libfunc(LibHandle lib, const char *symbol);
void os_closelib(LibHandle lib);

MSCBL_API OSString os_select_dir(const OSchar *title, const OSchar *default_path);
B32 os_path_exists(String path);

MSCBL_API void *os_reserve(void *ptr, U64 size);
MSCBL_API void os_release(void *ptr, U64 size);
MSCBL_API void os_commit(void *ptr, U64 size);
MSCBL_API void os_decommit(void *ptr, U64 size);

Semaphore os_semaphore_alloc(S32 initial, S32 max);
void os_semaphore_release(Semaphore s);
void os_semaphore_drop(Semaphore s);
B32 os_semaphore_take(Semaphore s, U64 end_us);

void os_thread_detach(Thread t);

typedef U32 FileAccess;
enum
{
    FileAccess_Read = (1 << 0),
    FileAccess_Write = (1 << 1),
};

enum FileMode
{
    FileMode_CreateAlways = (1 << 0),
    FileMode_OpenAlways = (1 << 1),
};

FileHandle os_file_open(String path, FileAccess access, FileMode mode);
void os_file_close(FileHandle file_desc);
U64 os_file_write(FileHandle file_desc, U64 size, U8 *buffer);
void os_file_read(FileHandle file_desc, U64 size, U8 *buffer);
U64 os_file_size(FileHandle file_desc);

#define os_map_get_data(map) (map.data)
// NOTE: Linux requires file to be of `size` size, so use `ftruncate` first
OSMmap os_file_map(FileHandle file_desc, U64 size);
void os_file_unmap(OSMmap map);
