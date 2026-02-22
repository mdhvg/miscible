#include <time.h>

#include "arena.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "base/threadpool.h"

static U64 start_t = 0;

void threadpool_run_tasks(ThreadPool *pool, Worker *worker)
{
    for (;;)
    {
        S64 task_left = ins_atomic_u64_dec_eval(&pool->task_left);

        if (task_left < 0)
        {
            break;
        }

        Arena *arena = pool->worker_arena.v[worker->id];
        pool->pool_func(arena, task_left, worker->id, pool->pool_data);

        U64 task_done = ins_atomic_u64_inc_eval(&pool->task_done);
        pool->busy    = task_done != pool->task_count;
        if (!pool->busy)
        {
            mscbl_log("Elapsed time: %.6f", ((double)(clock() - start_t)) / CLOCKS_PER_SEC);
            pool->available = 1;
        }
    }
}

OS_THREAD_ROUTINE(threadpool_worker)
{
    Worker *worker   = (Worker *)data;
    ThreadPool *pool = worker->pool;
    for (; pool->active;)
    {
        if (os_semaphore_take(pool->task_semaphore, U64_MAX))
        {
            threadpool_run_tasks(pool, worker);
        }
    }
    return 0;
}

OS_THREAD_ROUTINE(async_worker)
{
    Worker *worker   = (Worker *)data;
    ThreadPool *pool = worker->pool;
    for (; pool->active;)
    {
        if (os_semaphore_take(pool->async_semaphore, U64_MAX))
        {
            pool->async_busy = 1;
            pool->async_func(pool->async_arena, 0, 0, pool->async_data);
            pool->async_busy = 0;
        }
    }
    return 0;
}

ThreadPool *threadpool_init(Arena *arena, U32 worker_count)
{
    Semaphore task_semaphore = {0};

    task_semaphore = os_semaphore_alloc(0, S32_MAX);

    ThreadPool *pool = push_struct(arena, ThreadPool);
    *pool            = {0};

    pool->task_semaphore = task_semaphore;
    pool->active         = 1;
    pool->worker_count   = worker_count;
    pool->worker_array   = push_array(arena, worker_count, Worker);
    pool->worker_arena   = arena_array_alloc(GB(1), worker_count);

    arena_alloc(GB(1), pool->async_arena);
    pool->async_worker        = {.id = 0, .pool = pool};
    pool->async_worker.handle = os_thread_launch(async_worker, &pool->async_worker);
    pool->async_semaphore     = os_semaphore_alloc(0, S32_MAX);

    pool->available = 1;

    for (U64 i = 0; i < worker_count; i++)
    {
        Worker *worker = &pool->worker_array[i];
        worker->id     = i;
        worker->pool   = pool;
    }

    for (U64 i = 0; i < worker_count; i += 1)
    {
        Worker *worker = &pool->worker_array[i];
        worker->handle = os_thread_launch(threadpool_worker, worker);
    }

    return pool;
}

void threadpool_free(ThreadPool *pool)
{
    pool->active = 0;
    for (U64 i = 0; i < pool->worker_count; ++i)
    {
        os_semaphore_drop(pool->task_semaphore);
    }
    for (U64 i = 1; i < pool->worker_count; i += 1)
    {
        os_thread_detach(pool->worker_array[i].handle);
    }
    os_semaphore_release(pool->task_semaphore);
    MemoryZeroStruct(pool);
}

B32 async_job(ThreadPool *pool, thread_func *func, void *data)
{
    if (pool->async_busy)
        return 0;

    pool->async_func = func;
    pool->async_data = data;
    os_semaphore_drop(pool->async_semaphore);
    return 1;
}

B32 parallel_for(ThreadPool *pool, U64 task_count, thread_func *func, void *data)
{
    start_t = clock();
    if (task_count <= 0)
        return 0;
    if (ins_atomic_u64_eval(&pool->task_done) != pool->task_count)
        return 0;

    pool->pool_func  = func;
    pool->pool_data  = data;
    pool->task_count = task_count;
    pool->task_done  = 0;
    pool->available  = 0;
    pool->busy       = 1;
    ins_atomic_u64_eval_assign(&pool->task_left, task_count);

    U64 drop_count = MIN(task_count, pool->worker_count);

    for (U64 i = 0; i < drop_count; i++)
    {
        os_semaphore_drop(pool->task_semaphore);
    }
    return 1;
}
