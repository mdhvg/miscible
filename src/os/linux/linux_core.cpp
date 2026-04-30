#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>

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

const char *os_gethome()
{
    return getenv("HOME");
}

void os_cleanup()
{
}

void os_mkdir(String path)
{
    struct stat st = {};
    if (stat(CStrCast(path), &st) == -1)
    {
        Assert(mkdir(CStrCast(path), 0755) != -1);
    }
}

const char *os_select_dir(const char *title, const char *default_path)
{
    return tinyfd_selectFolderDialog(title, default_path);
}

void *os_reserve(void *ptr, U64 size)
{
    if (!size) return 0;
    void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED)
    {
        return 0;
    }
    return result;
}

void os_release(void *ptr, U64 size)
{
    B32 result = (munmap(ptr, size) != -1);
    OSAssert(result);
}

void os_commit(void *ptr, U64 size)
{
    if (!size) return;
    B32 result = (mprotect(ptr, size, PROT_READ | PROT_WRITE) != -1);
    OSAssert(result);
}

void os_decommit(void *ptr, U64 size)
{
    U32 result = madvise(ptr, size, MADV_DONTNEED);
    result |= mprotect(ptr, size, PROT_NONE);
    OSAssert(result != -1);
}

Semaphore os_semaphore_alloc(S32 initial, S32 max)
{
    Semaphore result = {0};
    sem_t *s         = (sem_t *)mmap(NULL, sizeof(*s), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    OSAssert(s != MAP_FAILED);
    S32 err = sem_init(s, 0, initial);
    OSAssert(!err);
    result.u64[0] = (U64)s;
    return result;
}

void os_semaphore_release(Semaphore s)
{
    S32 err = munmap((void *)s.u64[0], sizeof(sem_t));
    OSAssert(err == 0);
}

void os_semaphore_drop(Semaphore s)
{
    for (;;)
    {
        S32 err = sem_post((sem_t *)s.u64[0]);
        if (err == 0)
        {
            break;
        }
        else if (errno == EAGAIN)
        {
            continue;
        }
        break;
    }
}

B32 os_semaphore_take(Semaphore s, U64 end_us)
{
    OSAssert(end_us == U64_MAX);
    for (;;)
    {
        S32 err = sem_wait((sem_t *)s.u64[0]);
        if (err == 0)
        {
            break;
        }
        else if (errno == EAGAIN)
        {
            continue;
        }
        break;
    }
    return 1;
}

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, Worker *worker)
{
    pthread_t pth;
    S32 err = pthread_create(&pth, 0, fn, worker);
    OSAssert(err != -1);
    Thread t = {0};
    t.u64[0] = (U64)pth;
    return t;
}

void os_thread_detach(Thread t)
{
    pthread_t pth = (pthread_t)t.u64[0];
    S32 res       = pthread_detach(pth);
    if (res)
    {
        mscbl_log_error(Stringify(__func__), "Error code 0x%X (%lu)", res, res);
        Assert(0);
    }
}
