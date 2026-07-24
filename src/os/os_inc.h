// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

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

typedef union Guid Guid;
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

typedef struct FileMTime FileMTime;
struct FileMTime
{
    String path;
    U64 mtime;
};

#if OS_WIN32
#include "os/win32/win32_core.h"
#elif OS_LINUX
#include "os/linux/linux_core.h"
#endif

#ifndef OSChar
#error "OSChar not defined"
#endif
#ifndef OSSlash
#error "OSSlash not defined"
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
MSCBL_API void os_prelaunch();
void os_cleanup();

String os_env_var(const char *name, Arena *arena);
const char *os_gethome();
void os_mkdirs(String path);
U64 os_get_timestamp();
Time os_get_localtime();

U64 os_get_ticks_now();
U64 os_get_ticks_freq();

LibHandle os_loadlib(const char *filename);
LibAddress os_libfunc(LibHandle lib, const char *symbol);
void os_closelib(LibHandle lib);

MSCBL_API OSString os_select_dir(const OSChar *title, const OSChar *default_path, Arena *arena);
B32 os_path_exists(String path, Result *res);

MSCBL_API void *os_reserve(void *ptr, U64 size);
MSCBL_API void os_release(void *ptr, U64 size);
MSCBL_API void os_commit(void *ptr, U64 size);
MSCBL_API void os_decommit(void *ptr, U64 size);

Semaphore os_semaphore_init(S32 initial, S32 max);
void os_semaphore_destroy(Semaphore s);
void os_semaphore_push(Semaphore s);
B32 os_semaphore_pop(Semaphore s, U64 end_us);

void os_mutex_init(Mutex *mutex);
void os_mutex_destroy(Mutex *mutex);
void os_mutex_lock(Mutex *mutex);
void os_mutex_unlock(Mutex *mutex);
B32 os_mutex_trylock(Mutex *mutex);

void os_thread_detach(Thread t);
void os_thread_join(Thread t);

typedef U32 FileAccess;
enum
{
    FileAccess_Read = (1 << 0),
    FileAccess_Write = (1 << 1),
    FileAccess_Append = (1 << 2),
};

typedef enum FileMode FileMode;
enum FileMode
{
    FileMode_CreateAlways = (1 << 0),
    FileMode_OpenAlways = (1 << 1),
};

FileHandle _os_file_open(String path, FileAccess access, FileMode mode, Result *res, U64 size);
#if LANG_CPP
inline FileHandle os_file_open(String path, FileAccess access, FileMode mode, Result *res, U64 size = 0)
{
    return _os_file_open(path, access, mode, res, size);
}
#else
inline FileHandle os_file_open(String path, FileAccess access, FileMode mode, Result *res)
{
    return _os_file_open(path, access, mode, res, 0);
}
inline FileHandle os_file_open_resize(String path, FileAccess access, FileMode mode, Result *res, U64 size)
{
    return _os_file_open(path, access, mode, res, size);
}
#endif
void os_file_delete(String path, Result *res);
void os_file_close(FileHandle file_desc, Result *res);
U64 _os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset);
#if LANG_CPP
inline U64 os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset = 0)
{
    return _os_file_write(file_desc, size, buffer, res, offset);
}
#else
inline U64 os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res)
{
    return _os_file_write(file_desc, size, buffer, res, 0);
}
inline U64 os_file_write_at(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
{
    return _os_file_write(file_desc, size, buffer, res, offset);
}
#endif
U32 _os_file_read(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset);
#if LANG_CPP
inline U32 os_file_read(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset = 0)
{
    return _os_file_read(file_desc, size, buffer, res, offset);
}
#else
inline U32 os_file_read(FileHandle file_desc, U64 size, U8 *buffer, Result *res)
{
    return _os_file_read(file_desc, size, buffer, res, 0);
}
inline U32 os_file_read_at(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
{
    return _os_file_read(file_desc, size, buffer, res, offset);
}
#endif
U64 os_file_size(FileHandle file_desc, Result *res);
void os_file_rename(String old_path, String new_path);
FileMTime *os_list_by_pattern(String pattern, String base, Arena *arena);

#define os_map_get_data(map) (map.data)
// NOTE: Linux requires file to be of `size` size, so use `ftruncate` first
OSMmap os_file_map(FileHandle file_desc, U64 size, Result *res);
void os_file_unmap(OSMmap map, Result *res);
