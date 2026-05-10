#include <time.h>

#include "arena.h"
#include "base/ringbuf.h"
// #include "mscbl/task_graph.h"
// #include "mscbl/tp_task.h"
#include "os/win32/win32_core.h"
#include "base/threadpool.h"

local_v ThreadPool *pool = NULL;
local_v Arena *threadpool_arena = NULL;

ThreadFunc(clear_arena)
{
    arena_clear(arena);
}

void threadpool_run_tasks(Worker *worker)
{
    if (!pool || !pool->active || !rb_getsize(pool->tasks))
        return;

    Arena *arena = pool->worker_arena.v[worker->id];
    // S64 todo       = ins_atomic_u64_inc_eval(&pool->task_done);
    // AsyncTask task = pool->tasks[todo - 1];

    EnterCriticalSection(&pool->task_mutex);
    AsyncTask task = rb_pop(pool->tasks);
    LeaveCriticalSection(&pool->task_mutex);

    task.func(arena, worker->id, task.data);

    if (task.batch_size)
    {
        U64 left = ins_atomic_u64_dec_eval(task.batch_size);
        if (!left)
            os_semaphore_drop(task.batch_complete);
    }
}

OS_THREAD_ROUTINE(threadpool_worker)
{
    Worker *worker = (Worker *)data;
    for (; pool && pool->active;)
    {
        if (os_semaphore_take(pool->task_semaphore, U64_MAX))
            threadpool_run_tasks(worker);
    }
    return 0;
}

void threadpool_init(U32 worker_count)
{
    arena_alloc(KB(200), threadpool_arena);
    pool = push_struct0(threadpool_arena, ThreadPool);

    pool->active = 1;

    pool->task_semaphore = os_semaphore_alloc(0, S32_MAX);
    // pool->graph_semaphore = os_semaphore_alloc(0, S32_MAX);
    InitializeCriticalSection(&pool->task_mutex);
    pool->tasks = {0};

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

void threadpool_enqueue(AsyncTask task)
{
    EnterCriticalSection(&pool->task_mutex);
    rb_push(pool->tasks, task);
    LeaveCriticalSection(&pool->task_mutex);

    os_semaphore_drop(pool->task_semaphore);
}

void _threadpool_free()
{
    (pool)->active = 0;

    for (U64 i = 0; i < pool->worker_count; ++i)
        os_semaphore_drop(pool->task_semaphore);
    for (U64 i = 1; i < pool->worker_count; i += 1)
        os_thread_detach(pool->worker_array[i].handle);

    os_semaphore_release(pool->task_semaphore);
    DeleteCriticalSection(&pool->task_mutex);
    MemoryZeroStruct(pool);
    pool = NULL;
    arena_free(threadpool_arena);
}
#define threadpool_free() _threadpool_free()

// TODO: This is just a temporary fix. It can fail to clear all thread arenas.
void threadpool_clear_arenas()
{
    Semaphore batch = os_semaphore_alloc(0, S32_MAX);
    if (!pool)
        return;
    S64 jobs = pool->worker_count * 2;
    TPData args = {.kind = TPData_ANY, .any = NULL};
    for (U64 i = 0; i < pool->worker_count * 2; i++)
        threadpool_enqueue({.func = clear_arena,
                            .data = args,
                            .batch_size = &jobs,
                            .batch_complete = batch});
    os_semaphore_take(batch, U64_MAX);
    os_semaphore_drop(batch);
}
