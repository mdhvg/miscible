// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/random.h>

#include "base/array.h"
#include "base/log.h"
#include "base/string.h"
#include "os/os_inc.h"

Guid os_make_guid()
{
    Guid guid = {0};
    getrandom(guid.v, sizeof(guid.v), 0);
    guid.data3 &= 0x0fff;
    guid.data3 |= (4 << 12);
    guid.data4[0] &= 0x3f;
    guid.data4[0] |= 0x80;
    return guid;
}

void os_prelaunch()
{
    os_info.page_size = (U64)getpagesize();
}

// const char *os_gethome()
// {
//     return getenv("HOME");
// }

void os_cleanup()
{
}

String os_env_var(const char *name, Arena *arena)
{
    if (!name || !arena) return {0};

    char *value = getenv(name);
    if (!value) return {0};

    return string_copy(arena, sv(value));
}

void os_mkdirs(String path)
{
    Assert(path.size < KB(4), "Path is too long for buffer");

    char buffer[KB(4)] = {0};
    MemoryCopy(buffer, path.v, path.size);

    for (U64 i = 0; i < path.size; i++)
    {
        if (buffer[i] == '/')
        {
            buffer[i] = 0;
            if (i > 0)
            {
                B32 result = mkdir(buffer, 0777);
                B32 err = errno;

                Assert(result == 0 || err == EEXIST, "Error code: %u (%s)", err, buffer);
            }
            buffer[i] = '/';
        }
    }

    B32 result = mkdir(buffer, 0777);
    B32 err = errno;
    Assert(result == 0 || err == EEXIST, "Error code: %u (%s)", err, buffer);
}

U64 os_get_timestamp()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (U64)ts.tv_sec;
}

Time os_get_localtime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t raw_time = tv.tv_sec;
    struct tm local_time;

    localtime_r(&raw_time, &local_time);

    return (Time){
        .date = (U32)local_time.tm_mday,
        .month = (Month)local_time.tm_mon,
        .year = local_time.tm_year + 1900,

        .hour = (U32)local_time.tm_hour,
        .minute = (U32)local_time.tm_min,
        .second = (U32)local_time.tm_sec,
        .milsec = (U32)(tv.tv_usec / 1000),
    };
}

U64 os_get_ticks_now()
{
    struct timespec ts;
    S32 result = clock_gettime(CLOCK_MONOTONIC, &ts);
    Assert(result == 0, "Error Code: %u", errno);
    return ((U64)ts.tv_sec * 1000000000ULL) + (U64)ts.tv_nsec;
}

// void os_mkdirs(String path)
// {
//     struct stat st = {};
//     if (stat(CStrCast(path), &st) == -1)
//     {
//         Assert(mkdir(CStrCast(path), 0755) != -1);
//     }
// }

// const char *os_select_dir(const char *title, const char *default_path)
// {
//     return tinyfd_selectFolderDialog(title, default_path);
// }

#define _UnixTrap(res, cnd, err)      \
    do                                \
    {                                 \
        if (!(cnd) && res)            \
        {                             \
            *(res) = {                \
                .success = 0,         \
                .domain = Domain_OS,  \
                .code = (err),        \
                .context = __func__}; \
        }                             \
    } while (0)
#define UnixTrap(res, cnd) _UnixTrap(res, cnd, errno)

B32 os_path_exists(String path, Result *res)
{
    ClearResult(res);
    if (access(CStrCast(path), F_OK) != 0)
    {
        int err = errno;
        _UnixTrap(res, (err == ENOENT), err);
        return false;
    }

    return true;
}

void *os_reserve(void *ptr, U64 size)
{
    if (!size) return 0;
    void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    Assert(result != MAP_FAILED, "Error Code: %u", errno);
    return result;
}

void os_release(void *ptr, U64 size)
{
    B32 result = (munmap(ptr, size) != -1);
    Assert(result, "Error Code: %u", errno);
}

void os_commit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (mprotect(ptr, size, PROT_READ | PROT_WRITE) != -1);
    Assert(result, "Error Code: %u", errno);
}

void os_decommit(void *ptr, U64 size)
{
    U32 result = madvise(ptr, size, MADV_DONTNEED);
    result |= mprotect(ptr, size, PROT_NONE);
    Assert(result != -1, "Error Code: %u", errno);
}

Semaphore os_semaphore_init(S32 initial, S32 max)
{
    Semaphore sem = {0};
    Assert(!sem_init(&sem, 0, 0), "Error Code: %u", errno);
    return sem;
}

void os_semaphore_destroy(Semaphore s)
{
    Assert(!sem_destroy(&s), "Error Code: %u", errno);
}

void os_semaphore_push(Semaphore s)
{
    Assert(sem_post(&s), "Error Code: %u", errno);
}

B32 os_semaphore_pop(Semaphore s, U64 end_us)
{
    if (end_us == U64_MAX)
    {
        S32 res = sem_wait(&s);
        Assert(res == 0, "Error Code: %u", errno);
        return (res == 0);
    }

    struct timespec ts = {0};
    ts.tv_sec = (end_us / 1000000);
    ts.tv_nsec = (end_us % 1000000) * 1000;
    S32 res = sem_timedwait(&s, &ts);
    Assert(res == 0, "Error Code: %u", errno);
    return (res == 0);
}

void os_mutex_init(Mutex *mutex)
{
    pthread_mutex_init(mutex, NULL);
}

void os_mutex_destroy(Mutex *mutex)
{
    pthread_mutex_destroy(mutex);
}

void os_mutex_lock(Mutex *mutex)
{
    pthread_mutex_lock(mutex);
}

void os_mutex_unlock(Mutex *mutex)
{
    pthread_mutex_unlock(mutex);
}

B32 os_mutex_trylock(Mutex *mutex)
{
    return pthread_mutex_trylock(mutex);
}

Thread os_thread_launch(ThreadRoutine_t fn, struct Worker *worker)
{
    Thread t;
    S32 result = pthread_create(&t, NULL, fn, worker);
    Assert(result == 0, "Error Code: %u", result);
    return t;
}

void os_thread_detach(Thread t)
{
    pthread_detach(t);
}

FileHandle _os_file_open(String path, FileAccess access, FileMode mode, Result *res, U64 size)
{
    ClearResult(res);

    S32 flags = 0;
    B32 has_read = (access & FileAccess_Read) != 0;
    B32 has_write = (access & FileAccess_Write) != 0;
    B32 has_append = (access & FileAccess_Append) != 0;

    if (has_read && (has_write || has_append))
        flags |= O_RDWR;
    else if (has_write || has_append)
        flags |= O_WRONLY;
    else if (has_read)
        flags |= O_RDONLY;
    else
        Assert(0, "needs to have read/write/both access");

    if (has_append) flags |= O_APPEND;

    switch (mode)
    {
    case FileMode_CreateAlways:
        flags |= (O_CREAT | O_TRUNC);
        break;
    case FileMode_OpenAlways:
        flags |= O_CREAT;
        break;
    default:
        Assert(0, "invalid file open mode %u", mode);
    }

    FileHandle fd = open(CStrCast(path), flags, 0666);
    UnixTrap(res, fd != -1);
    return fd;
}

void os_file_delete(String path, Result *res)
{
    ClearResult(res);
    B32 out = unlink(CStrCast(path));
    UnixTrap(res, out == 0);
}

void os_file_close(FileHandle file_desc, Result *res)
{
    ClearResult(res);
    if (file_desc < 0) return;
    B32 out = close(file_desc);
    UnixTrap(res, out == 0);
}

U64 _os_file_write(FileHandle file_desc, U64 size, U8 *buffer, Result *res, U64 offset)
{
    ClearResult(res);
    S64 written = pwrite(file_desc, buffer, size, offset);
    UnixTrap(res, written >= 0);
    return MAX(written, 0);
}

FileMtime *os_list_dir(String dir, Arena *arena)
{
    FileMtime *res = NULL;

    S32 dirfd = open(CStrCast(dir), O_RDONLY | O_DIRECTORY);
    DIR *dirp = fdopendir(dirfd);
    dirent64 *file_entry = readdir64(dirp);
    do
    {
        if (!file_entry) break;

        String filename = sv(file_entry->d_name);

        // Skip current and parent dir pointers
        if (string_match_beg(filename, sv(".")) || string_match_beg(filename, sv(".."))) continue;

        if (file_entry->d_type == DT_DIR)
        {
            struct statx stat;
            Assert(statx(AT_FDCWD, CStrCast(dir), AT_SYMLINK_NOFOLLOW, STATX_TYPE | STATX_SIZE | STATX_MTIME, &stat) != -1, "Error Code: %u (Dir: %.*s)", errno, StringSpr(dir));

            StringBuilder file_path = string_init(arena, dir);
            path_join(&file_path, filename);
            FileMtime entry = {.path = StringCast(file_path), .mtime = (U64)stat.stx_mtime.tv_sec};
            da_push(arena, res, entry);
        }
    } while (readdir64(dirp));
    close(dirfd);
    return res;
}

void os_thread_join(Thread t)
{
    S32 result = pthread_join(t, NULL);
    Assert(result == 0, "Error Code: %u", result);
}
