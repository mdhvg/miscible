#pragma once
#include "base/arena.h"
#include "base/base_core.h"
#include "base/threads.h"

#define THREAD_FUNC(name) void name(Arena *arena, U64 n, U64 id, void *data)
typedef THREAD_FUNC(thread_func);

struct Worker
{
    U64 id;
    struct ThreadPool *pool;
    Thread handle;
};

struct ThreadPool
{
    B32 active;
    B32 busy;

    U64 task_count;
    U64 task_done;
    S64 task_left;
    B32 available;
    U32 worker_count;
    Semaphore task_semaphore;
    Worker *worker_array;
    ArenaArray worker_arena;
    thread_func *pool_func;
    void *pool_data;

    Worker async_worker;
    thread_func *async_func;
    void *async_data;
    B32 async_busy;
};

ThreadPool *threadpool_init(Arena *arena, U32 worker_count);
void threadpool_free(ThreadPool *pool);
B32 async_job(ThreadPool *pool, thread_func *func, void *data);
B32 parallel_for(ThreadPool *pool, U64 task_count, thread_func *func, void *data);
