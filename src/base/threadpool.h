#pragma once
#include "os/os_inc.h"
#include "base/arena.h"
#include "base/ringbuf.h"

#define ThreadFunc(name) void name(Arena *arena, U64 id, struct TPData data)
typedef ThreadFunc(thread_func);

struct Worker
{
    U64 id;
    Thread handle;
};

enum TPData_
{
    TPData_ANY,
    TPData_String,
    TPData_OSString,
    TPData_COUNT
};

struct TPData
{
    TPData_ kind;
    union {
        void *val_any;
        String val_str;
        OSString val_os_str;
    };
};

struct AsyncTask
{
    thread_func *func;
    TPData data;

    volatile S64 *batch_size;
    Semaphore batch_complete;
};

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
    RingBuffer(AsyncTask, tasks_p0, KB(1));
    RingBuffer(AsyncTask, tasks_p1, KB(1));
    RingBuffer(AsyncTask, tasks_p2, KB(1));
};

void threadpool_init(U32 worker_count);
void threadpool_free();
void threadpool_clear_arenas();

enum TaskPriority
{
    TaskPriority_Realtime,
    TaskPriority_High,
    TaskPriority_Low,
};

MSCBL_API void threadpool_enqueue(TaskPriority priority, AsyncTask task);
