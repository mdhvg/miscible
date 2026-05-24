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

void threadpool_init(U32 worker_count);
void threadpool_free();
U64 threadpool_worker_count();

enum TaskPriority
{
    TaskPriority_Realtime,
    TaskPriority_High,
    TaskPriority_Low,
};

MSCBL_API void threadpool_enqueue(TaskPriority priority, AsyncTask task);
