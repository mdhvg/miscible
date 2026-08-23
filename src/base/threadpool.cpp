// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "arena.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "base/ringbuf.h"
#include "base/threadpool.h"

struct ThreadPool
{
    B32 active;

    Semaphore task_semaphore;
    Mutex task_mutex;

    U32 worker_count;
    Worker *worker_array;
    ArenaArray worker_arena;

    // NOTE: Priority level based task buffers with 3 levels of priority. Push
    // to any buffer is considered as a go signal for workers to release only
    // difference is, while popping the latest task, they check in the order
    // 0 -> 1 -> 2 and hence will always finish the most important tasks first
    RingBuffer_t(AsyncTask) tasks_p0;
    RingBuffer_t(AsyncTask) tasks_p1;
    RingBuffer_t(AsyncTask) tasks_p2;
};

local_v ThreadPool *pool = NULL;
local_v Arena *threadpool_arena = NULL;

ThreadFunc(clear_arena)
{
    arena_clear(arena);
}

void threadpool_run_tasks(Worker *worker)
{
    if (!pool || !pool->active)
        return;

    os_mutex_lock(&pool->task_mutex);
    if (rb_isempty(pool->tasks_p0) && rb_isempty(pool->tasks_p1) && rb_isempty(pool->tasks_p2))
        return;
    os_mutex_unlock(&pool->task_mutex);

    Arena *arena = pool->worker_arena.v[worker->id];
    AsyncTask task = {0};

    os_mutex_lock(&pool->task_mutex);
    if (!rb_isempty(pool->tasks_p0))
    {
        rb_top(pool->tasks_p0, &task);
        rb_pop(pool->tasks_p0);
    }
    else if (!rb_isempty(pool->tasks_p1))
    {
        rb_top(pool->tasks_p1, &task);
        rb_pop(pool->tasks_p1);
    }
    else
    {
        rb_top(pool->tasks_p2, &task);
        rb_pop(pool->tasks_p2);
    }
    os_mutex_unlock(&pool->task_mutex);

    task.func(arena, worker->id, task.args);

    if (task.batch_size)
    {
        S64 left = ins_atomic_u64_dec_eval(task.batch_size);
        if (left <= 0)
            os_semaphore_push(task.batch_complete);
    }
}

OS_THREAD_ROUTINE(threadpool_worker)
{
    Worker *worker = (Worker *)data;
    for (; pool && pool->active;)
    {
        if (os_semaphore_pop(pool->task_semaphore, U64_MAX))
            threadpool_run_tasks(worker);
    }
    return 0;
}

void threadpool_init(U32 worker_count)
{
    arena_alloc(MB(1), threadpool_arena);
    pool = push_struct0(threadpool_arena, ThreadPool);

    pool->active = 1;

    pool->task_semaphore = os_semaphore_init(0, S32_MAX);
    rb_init(threadpool_arena, pool->tasks_p0, KB(1));
    rb_init(threadpool_arena, pool->tasks_p1, KB(1));
    rb_init(threadpool_arena, pool->tasks_p2, KB(1));
    os_mutex_init(&pool->task_mutex);

    pool->worker_count = worker_count;
    pool->worker_array = push_array(threadpool_arena, worker_count, Worker);
    pool->worker_arena = arena_array_alloc(MB(50), worker_count);

    for (U64 i = 0; i < worker_count; i++)
    {
        Worker *worker = &pool->worker_array[i];
        worker->id = i;
    }

    for (U64 i = 0; i < worker_count; i += 1)
    {
        Worker *worker = &pool->worker_array[i];
        worker->handle = os_thread_launch(threadpool_worker, worker);
    }
}

#if DBG
void _threadpool_enqueue(TaskPriority priority, AsyncTask task, const char *func_name)
#else
void threadpool_enqueue(TaskPriority priority, AsyncTask task)
#endif
{
#if DBG
    task.queue_func = func_name;
#endif
    os_mutex_lock(&pool->task_mutex);
    switch (priority)
    {
    case TaskPriority_Realtime:
        Assert(rb_push(pool->tasks_p0, task), "ringbuffer full");
        break;
    case TaskPriority_High:
        Assert(rb_push(pool->tasks_p1, task), "ringbuffer full");
        break;
    case TaskPriority_Low:
        Assert(rb_push(pool->tasks_p2, task), "ringbuffer full");
        break;
    }
    os_mutex_unlock(&pool->task_mutex);

    os_semaphore_push(pool->task_semaphore);
}

void threadpool_free()
{
    (pool)->active = 0;

    for (U64 i = 0; i < pool->worker_count; ++i)
        os_semaphore_push(pool->task_semaphore);
    for (U64 i = 0; i < pool->worker_count; i += 1)
        // os_thread_detach(pool->worker_array[i].handle);
        os_thread_join(pool->worker_array[i].handle);
    // NOTE: Switched to thread join instead of detatch for now, this might be
    // the culprit if some lag while closing shows up

    os_semaphore_destroy(pool->task_semaphore);
    os_mutex_destroy(&pool->task_mutex);
    MemoryZeroStruct(pool);
    pool = NULL;
    arena_free(threadpool_arena);
}

U64 threadpool_worker_count()
{
    return pool->worker_count;
}
